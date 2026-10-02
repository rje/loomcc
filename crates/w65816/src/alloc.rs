//! Value homes: which values stay in the accumulator between two
//! instructions, which get a direct-page word (the 816-tcc scratch
//! registers $00-$17 and $20-$27), and which live in the function's static
//! frame (the compiled stack, bank $7E).

use crate::analysis::*;
use loomcc_ir::*;

/// Direct-page words the allocator may hand out. $18-$1F (tcc__r9/r10)
/// are the code generator's own scratch.
pub const DP_POOL: &[u8] = &[0x00, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e, 0x10, 0x12, 0x14, 0x16, 0x20, 0x22, 0x24, 0x26];
/// Scratch: one pointer ($1c-$1f, tcc__r10) and one word pair ($18-$1b).
pub const SCRATCH_PTR: u8 = 0x1c;
pub const SCRATCH_PTR2: u8 = 0x18;
pub const SCRATCH_WORD: u8 = 0x18;
/// Where A is parked while an address is prepared.
pub const SAVE_A: u8 = 0x1a;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Home {
    /// Not stored anywhere (dead, or consumed straight from A).
    None,
    Dp(u8),
    /// Byte offset in the function's static frame.
    Frame(u32),
    /// The X index register (8/16-bit values only).
    X,
    /// The Y index register.
    Y,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum IdxReg {
    X,
    Y,
}

/// Does the code generator use `r` while emitting `inst`, other than to
/// read or write `v` (the candidate living in `r`) as its own index?
/// Must agree with isel.rs; when in doubt, say yes.
pub fn inst_uses_reg(f: &Func, inst: &Inst, r: IdxReg, v: VReg) -> bool {
    inst_uses_reg_ext(f, inst, r, v, false)
}

/// `slot_x`: frame slots may lie beyond direct-page reach of a frame on the
/// hardware stack, where any slot access goes through X (dynframe).
pub fn inst_uses_reg_ext(f: &Func, inst: &Inst, r: IdxReg, v: VReg, slot_x: bool) -> bool {
    let addr_uses = |a: &Addr, width2: bool| -> bool {
        match &a.base {
            Base::Reg(_) => {
                let neg = a.offset < 0 || a.offset >= 0x8000;
                match r {
                    IdxReg::Y => match a.index {
                        None => !neg && (a.offset != 0 || width2),
                        Some((i, s)) => !(i == v && s == 1 && a.offset == 0 && !width2),
                    },
                    // Staging a pointer that is not in direct page keeps a
                    // forwarded index in X; a wide load through a pointer
                    // may use X to avoid overwriting its own pointer.
                    IdxReg::X => width2 || a.index.map_or(false, |(i, _)| i != v),
                }
            }
            Base::Slot(_) if slot_x => r == IdxReg::X,
            _ => match r {
                IdxReg::X => match a.index {
                    None => false,
                    Some((i, s)) => !(i == v && s == 1),
                },
                IdxReg::Y => false,
            },
        }
    };
    let w2 = |t: IrTy| matches!(t, IrTy::I32 | IrTy::Ptr);
    match inst {
        Inst::Call { .. } | Inst::Memcpy { .. } | Inst::Memset { .. } => true,
        Inst::Load { dst, addr, .. } => addr_uses(addr, w2(f.ty(*dst))),
        Inst::Store { addr, ty, .. } => addr_uses(addr, w2(*ty)),
        Inst::Lea { .. } => false,
        Inst::Bin { op, a, b, dst } => {
            let t = f.ty(*dst);
            match op {
                BinOp::Shl | BinOp::ShrS | BinOp::ShrU => b.imm().is_none() && r == IdxReg::Y,
                BinOp::Mul => a.imm().is_none() && b.imm().is_none() && r == IdxReg::X || t == IrTy::I32,
                BinOp::DivU | BinOp::RemU | BinOp::DivS | BinOp::RemS => {
                    let pow2 = b.imm().map_or(false, |k| {
                        let k = k & 0xffff;
                        k != 0 && k & (k - 1) == 0
                    });
                    !(pow2 && matches!(op, BinOp::DivU | BinOp::RemU | BinOp::DivS)) && r == IdxReg::X || t == IrTy::I32
                }
                _ => t == IrTy::I32 && false,
            }
        }
        _ => false,
    }
}

pub fn term_uses_reg(f: &Func, t: &Term, r: IdxReg) -> bool {
    match t {
        Term::Switch { .. } => r == IdxReg::X,
        Term::Ret(Some(_)) => r == IdxReg::X && f.ret.map_or(false, |t| matches!(t, IrTy::I32 | IrTy::Ptr)),
        _ => false,
    }
}

pub struct Alloc {
    pub homes: Vec<Home>,
    /// Kept in A from its definition to its single use in the next
    /// instruction.
    pub forwarded: Vec<bool>,
    /// Loads used once, by the next instruction, as its second operand: the
    /// address, read in place by that instruction (`sbc [dp],y`).
    pub folded: Vec<Option<Addr>>,
    /// 8-bit values whose 16-bit home always has a zero high byte.
    pub clean8: Vec<bool>,
    pub uses: Vec<u32>,
    /// Frame layout.
    pub param_offsets: Vec<u32>,
    pub sret_offset: Option<u32>,
    pub slot_offsets: Vec<u32>,
    pub frame_size: u32,
    /// Direct-page words this function writes (for callers' allocation).
    pub dp_used: Vec<u8>,
    /// Parameters whose home is not their slot (copied at entry).
    pub param_copies: Vec<(u32, Home, IrTy)>,
}

fn words(t: IrTy) -> u32 {
    match t {
        IrTy::I8 | IrTy::I16 => 1,
        IrTy::I32 | IrTy::Ptr => 2,
    }
}

/// Does `inst` take the register `v` in a position the code generator can
/// feed from the accumulator?
fn accepts_a(f: &Func, inst: &Inst, v: VReg) -> bool {
    let is = |o: &Operand| o == &Operand::Reg(v);
    let narrow = |t: IrTy| matches!(t, IrTy::I8 | IrTy::I16);
    let count = inst.uses().iter().filter(|&&u| u == v).count();
    if count != 1 {
        return false;
    }
    match inst {
        Inst::Mov { dst, src } => is(src) && narrow(f.ty(*dst)),
        Inst::Bin { dst, a, b, .. } => (is(a) || is(b)) && narrow(f.ty(*dst)),
        Inst::Un { dst, a, .. } => is(a) && narrow(f.ty(*dst)),
        Inst::Cmp { ty, .. } => narrow(*ty),
        Inst::Conv { src, .. } => is(src),
        Inst::Store { src, addr, .. } => is(src) || addr.index.map_or(false, |(r, _)| r == v),
        Inst::Load { addr, .. } | Inst::Lea { addr, .. } => addr.index.map_or(false, |(r, _)| r == v) && addr.base != Base::Reg(v),
        _ => false,
    }
}

fn term_accepts_a(t: &Term, v: VReg) -> bool {
    let is = |o: &Operand| o == &Operand::Reg(v);
    match t {
        Term::Ret(Some(o)) => is(o),
        Term::Br { cond, .. } => is(cond),
        Term::BrCmp { ty, a, b, .. } => matches!(ty, IrTy::I8 | IrTy::I16) && (is(a) ^ is(b)),
        Term::Switch { val, .. } => is(val),
        _ => false,
    }
}

/// Does the defining instruction leave its result in A?
fn defines_in_a(f: &Func, inst: &Inst) -> bool {
    match inst {
        Inst::Mov { dst, .. } | Inst::Bin { dst, .. } | Inst::Un { dst, .. } | Inst::Conv { dst, .. } | Inst::Load { dst, .. } => {
            matches!(f.ty(*dst), IrTy::I8 | IrTy::I16)
        }
        Inst::Cmp { .. } => true,
        Inst::Call { dst: Some(d), .. } => matches!(f.ty(*d), IrTy::I8 | IrTy::I16),
        _ => false,
    }
}

/// `clobber` gives the direct-page words a call may overwrite (its callee
/// tree's words; everything for code outside the module).
pub fn allocate(f: &Func, dp_allowed: &[u8], clobber: &dyn Fn(&Callee) -> Vec<u8>) -> Alloc {
    allocate_ext(f, dp_allowed, clobber, false)
}

/// allocate, with frame-slot accesses using X (`slot_x`, see
/// inst_uses_reg_ext).
pub fn allocate_ext(f: &Func, dp_allowed: &[u8], clobber: &dyn Fn(&Callee) -> Vec<u8>, slot_x: bool) -> Alloc {
    let n = f.vregs.len();
    let uses = use_counts(f);
    let defs = def_counts(f);
    let lv = liveness(f);
    let is_param: Vec<bool> = {
        let mut v = vec![false; n];
        for r in f.param_regs.iter().flatten() {
            v[r.0 as usize] = true;
        }
        if let Some(r) = f.sret_reg {
            v[r.0 as usize] = true;
        }
        v
    };

    // 1. Forwarding and load folding.
    let mut forwarded = vec![false; n];
    let mut folded: Vec<Option<Addr>> = vec![None; n];
    for (bi, b) in f.blocks.iter().enumerate() {
        for (k, inst) in b.insts.iter().enumerate() {
            let Some(d) = inst.def() else { continue };
            if uses[d.0 as usize] != 1 || defs[d.0 as usize] != 1 || is_param[d.0 as usize] || lv.live_out[bi].get(d.0) {
                continue;
            }
            if !defines_in_a(f, inst) {
                continue;
            }
            // A load feeding the second operand of the next instruction is
            // read in place there.
            if let Inst::Load { dst, addr, volatile: false } = inst {
                if f.ty(*dst) == IrTy::I16 && addr.regs().iter().all(|r| !forwarded[r.0 as usize]) {
                    let is = |o: &Operand| o == &Operand::Reg(d);
                    let second = match b.insts.get(k + 1) {
                        Some(Inst::Bin { op: BinOp::Add | BinOp::Sub | BinOp::And | BinOp::Or | BinOp::Xor, dst: bd, a, b: bb }) => {
                            is(bb) && !is(a) && f.ty(*bd) == IrTy::I16
                        }
                        Some(Inst::Cmp { ty: IrTy::I16, a, b: bb, .. }) => is(bb) && !is(a),
                        Some(_) => false,
                        None => matches!(&b.term, Term::BrCmp { ty: IrTy::I16, a, b: bb, .. } if is(bb) && !is(a)),
                    };
                    if second {
                        folded[d.0 as usize] = Some(addr.clone());
                        continue;
                    }
                }
            }
            let ok = match b.insts.get(k + 1) {
                Some(next) => accepts_a(f, next, d),
                None => term_accepts_a(&b.term, d),
            };
            if ok {
                forwarded[d.0 as usize] = true;
            }
        }
    }

    // 2. Interference among the values that need homes.
    let needs: Vec<bool> = (0..n).map(|i| !forwarded[i] && folded[i].is_none() && (uses[i] > 0 || is_param[i])).collect();
    let mut adj: Vec<Vec<u32>> = vec![Vec::new(); n];
    let add_edge = |a: u32, b: u32, adj: &mut Vec<Vec<u32>>| {
        if a != b && !adj[a as usize].contains(&b) {
            adj[a as usize].push(b);
            adj[b as usize].push(a);
        }
    };
    let mut cross_call = vec![false; n];
    let mut forbid: Vec<Vec<u8>> = vec![Vec::new(); n];
    let weights = loop_weight(f);
    let mut score = vec![0u64; n];
    for (bi, b) in f.blocks.iter().enumerate() {
        let after = live_after(f, &lv, bi);
        for (k, inst) in b.insts.iter().enumerate() {
            for u in inst.uses() {
                score[u.0 as usize] += weights[bi] as u64;
            }
            if let Some(d) = inst.def() {
                score[d.0 as usize] += weights[bi] as u64;
                if needs[d.0 as usize] {
                    for o in after[k].iter() {
                        if needs[o as usize] {
                            add_edge(d.0, o, &mut adj);
                        }
                    }
                    // Multi-word results are written a word at a time: they
                    // must not share a home with an operand still being read.
                    let wide = words(f.ty(d)) == 2 || inst.uses().iter().any(|u| words(f.ty(*u)) == 2);
                    if wide {
                        for u in inst.uses() {
                            if needs[u.0 as usize] {
                                add_edge(d.0, u.0, &mut adj);
                            }
                        }
                    }
                }
            }
            if let Inst::Call { callee, .. } = inst {
                let words = clobber(callee);
                for o in after[k].iter() {
                    if Some(VReg(o)) != inst.def() {
                        cross_call[o as usize] = true;
                        for &wd in &words {
                            if !forbid[o as usize].contains(&wd) {
                                forbid[o as usize].push(wd);
                            }
                        }
                    }
                }
            }
            // Helpers the code generator calls (multiply, divide, variable
            // shifts) use only scratch; memcpy/memset loops likewise.
        }
        for u in b.term.uses() {
            score[u.0 as usize] += weights[bi] as u64;
        }
    }
    // Parameters and values live into the entry block interfere with each
    // other.
    let entry_live: Vec<u32> = lv.live_in[0].iter().chain((0..n as u32).filter(|&i| is_param[i as usize])).collect();
    for (i, &a) in entry_live.iter().enumerate() {
        for &b in &entry_live[i + 1..] {
            if needs[a as usize] && needs[b as usize] {
                add_edge(a, b, &mut adj);
            }
        }
    }

    // 2b. Index-register homes: loop counters and indexes that X or Y can
    // hold across their whole live range.
    let mut homes = vec![Home::None; n];
    {
        let mut index_use = vec![(0u64, 0u64); n]; // (score as Y, score as X)
        for (bi, b) in f.blocks.iter().enumerate() {
            for inst in &b.insts {
                let addr = match inst {
                    Inst::Load { addr, .. } | Inst::Store { addr, .. } => Some(addr),
                    _ => None,
                };
                if let Some(a) = addr {
                    if let Some((i, 1)) = a.index {
                        let wgt = weights[bi] as u64;
                        match a.base {
                            Base::Reg(_) => index_use[i.0 as usize].0 += wgt,
                            _ => index_use[i.0 as usize].1 += wgt,
                        }
                    }
                }
                // Counters: v = v +/- 1.
                if let Inst::Bin { op: BinOp::Add | BinOp::Sub, dst, a: Operand::Reg(x), b: Operand::Imm(k) } = inst {
                    if dst == x && (k & 0xffff == 1 || k & 0xffff == 0xffff) {
                        index_use[dst.0 as usize].0 += weights[bi] as u64;
                        index_use[dst.0 as usize].1 += weights[bi] as u64;
                    }
                }
            }
        }
        let mut in_reg: Vec<(u32, IdxReg)> = Vec::new();
        for reg in [IdxReg::Y, IdxReg::X] {
            let mut cands: Vec<u32> = (0..n as u32)
                .filter(|&v| needs[v as usize] && matches!(f.ty(VReg(v)), IrTy::I8 | IrTy::I16) && !cross_call[v as usize])
                .filter(|&v| {
                    let (y, x) = index_use[v as usize];
                    if reg == IdxReg::Y { y > 0 } else { x > 0 || y > 0 }
                })
                .filter(|&v| homes[v as usize] == Home::None)
                .collect();
            cands.sort_by_key(|&v| {
                let (y, x) = index_use[v as usize];
                std::cmp::Reverse(if reg == IdxReg::Y { y * 2 + x } else { x * 2 + y })
            });
            for v in cands {
                let clash = in_reg.iter().any(|&(o, r2)| r2 == reg && adj[v as usize].contains(&o));
                if clash || !reg_free(f, &lv, VReg(v), reg, slot_x) {
                    continue;
                }
                homes[v as usize] = if reg == IdxReg::X { Home::X } else { Home::Y };
                in_reg.push((v, reg));
            }
        }
    }

    // 3. Frame layout: parameter slots first.
    let mut off = 0u32;
    let mut param_offsets = Vec::new();
    for p in &f.params {
        param_offsets.push(off);
        off += match p {
            ParamKind::Scalar(t) => words(*t) * 2,
            ParamKind::Aggregate(sz) => sz.div_ceil(2) * 2,
        };
    }
    let sret_offset = if f.sret.is_some() {
        let o = off;
        off += 4;
        Some(o)
    } else {
        None
    };
    let spill_base = off;

    // 4. Colour: pointer bases and hot values first get direct page.
    let bases = pointer_bases(f);
    // Copy partners: the two sides of a `mov` between registers or of a
    // byte widened to 16 bits (the same home makes the copy free).
    let mut partners: Vec<Vec<u32>> = vec![Vec::new(); n];
    for b in &f.blocks {
        for i in &b.insts {
            let pair = match i {
                Inst::Mov { dst, src: Operand::Reg(s) } => Some((*dst, *s)),
                Inst::Conv { kind: ConvKind::Zext, dst, src: Operand::Reg(s), from: IrTy::I8 } => Some((*dst, *s)),
                _ => None,
            };
            if let Some((d, s)) = pair {
                if d != s {
                    partners[d.0 as usize].push(s.0);
                    partners[s.0 as usize].push(d.0);
                }
            }
        }
    }
    let mut order: Vec<u32> = (0..n as u32).filter(|&i| needs[i as usize]).collect();
    order.sort_by_key(|&i| {
        let b = bases[i as usize] as u64;
        std::cmp::Reverse((b << 40) + score[i as usize])
    });
    let mut dp_used: Vec<u8> = Vec::new();
    for &v in &order {
        if matches!(homes[v as usize], Home::X | Home::Y) {
            continue;
        }
        let t = f.ty(VReg(v));
        let w = words(t);
        let param_slot = f.param_regs.iter().position(|r| *r == Some(VReg(v)));
        {
            // Try direct page.
            // Precise: a neighbour's words.
            let mut busy: Vec<u8> = forbid[v as usize].clone();
            for &o in &adj[v as usize] {
                if let Home::Dp(d) = homes[o as usize] {
                    busy.push(d);
                    if words(f.ty(VReg(o))) == 2 {
                        busy.push(d + 2);
                    }
                }
            }
            let free = |d: u8| {
                let ok1 = dp_allowed.contains(&d) && !busy.contains(&d);
                if w == 1 {
                    ok1
                } else {
                    ok1 && dp_allowed.contains(&(d + 2)) && !busy.contains(&(d + 2))
                }
            };
            // A copy partner's word first (a `mov`, or a byte widened to
            // 16 bits): when they do not interfere the copy disappears.
            let partner = partners[v as usize].iter().find_map(|&p| match homes[p as usize] {
                Home::Dp(d) if words(f.ty(VReg(p))) == w && free(d) => Some(d),
                _ => None,
            });
            let pick = partner.or_else(|| dp_allowed.iter().copied().find(|&d| free(d)));
            if let Some(d) = pick {
                // A value used once and not in a loop is not worth a DP word
                // more than a frame word; take it anyway (DP is cheaper).
                homes[v as usize] = Home::Dp(d);
                if !dp_used.contains(&d) {
                    dp_used.push(d);
                }
                if w == 2 && !dp_used.contains(&(d + 2)) {
                    dp_used.push(d + 2);
                }
                continue;
            }
        }
        if let Some(pi) = param_slot {
            homes[v as usize] = Home::Frame(param_offsets[pi]);
            continue;
        }
        if f.sret_reg == Some(VReg(v)) {
            homes[v as usize] = Home::Frame(sret_offset.unwrap());
            continue;
        }
        // Frame words: lowest offset not used by an interfering neighbour.
        let mut busy: Vec<(u32, u32)> = Vec::new();
        for &o in &adj[v as usize] {
            if let Home::Frame(fo) = homes[o as usize] {
                busy.push((fo, fo + words(f.ty(VReg(o))) * 2));
            }
        }
        let mut cand = spill_base;
        loop {
            let end = cand + w * 2;
            if let Some(&(_, e)) = busy.iter().find(|&&(s, e)| cand < e && s < end) {
                cand = e;
                continue;
            }
            break;
        }
        homes[v as usize] = Home::Frame(cand);
        off = off.max(cand + w * 2);
    }
    // Parameters placed in DP or an index register are copied from their
    // slot at entry.
    let mut param_copies = Vec::new();
    for (pi, r) in f.param_regs.iter().enumerate() {
        if let Some(r) = r {
            if let Home::Dp(_) | Home::X | Home::Y = homes[r.0 as usize] {
                param_copies.push((param_offsets[pi], homes[r.0 as usize], f.ty(*r)));
            }
        }
    }
    if let (Some(r), Some(so)) = (f.sret_reg, sret_offset) {
        if let Home::Dp(_) = homes[r.0 as usize] {
            param_copies.push((so, homes[r.0 as usize], IrTy::Ptr));
        }
    }
    // 5. IR slots after the spills; a struct parameter's slot is its
    // parameter area (the caller or the ABI entry copied the bytes there).
    let mut slot_offsets = Vec::new();
    for (si, s) in f.slots.iter().enumerate() {
        if let Some(pi) = f.param_slots.iter().position(|p| *p == Some(SlotId(si as u32))) {
            slot_offsets.push(param_offsets[pi]);
            continue;
        }
        let a = s.align.clamp(1, 2);
        off = off.div_ceil(a) * a;
        slot_offsets.push(off);
        off += s.size;
    }
    let frame_size = off.div_ceil(2) * 2;
    let clean8 = clean8(f);
    Alloc { homes, forwarded, folded, clean8, uses, param_offsets, sret_offset, slot_offsets, frame_size, dp_used, param_copies }
}

/// Can `v` live in `reg` from its definitions to its last uses?
fn reg_free(f: &Func, lv: &Liveness, v: VReg, reg: IdxReg, slot_x: bool) -> bool {
    for (bi, b) in f.blocks.iter().enumerate() {
        let after = live_after(f, lv, bi);
        for (k, inst) in b.insts.iter().enumerate() {
            let used = inst.uses().contains(&v);
            let live_out = after[k].get(v.0);
            let defd = inst.def() == Some(v);
            let live_in = used || (live_out && !defd);
            if !live_in && !live_out && !defd {
                continue;
            }
            if live_in && inst_uses_reg_ext(f, inst, reg, v, slot_x) {
                return false;
            }
            // Wide (32-bit/pointer) results written through X.
            if defd && live_in && matches!(inst, Inst::Call { .. }) {
                return false;
            }
        }
        let t_live = b.term.uses().contains(&v) || lv.live_out[bi].get(v.0);
        if t_live && term_uses_reg(f, &b.term, reg) {
            return false;
        }
    }
    true
}

/// 8-bit registers whose every definition leaves a zero high byte in the
/// 16-bit home (constants, compare results, masks, zero-extended shifts).
pub fn clean8(f: &Func) -> Vec<bool> {
    clean8_ext(f, false)
}

/// `clean_params`: 8-bit parameters arrive with a zero high byte (a frame
/// on the hardware stack masks them in its prologue).
pub fn clean8_ext(f: &Func, clean_params: bool) -> Vec<bool> {
    let n = f.vregs.len();
    let mut c: Vec<bool> = (0..n).map(|i| f.vregs[i] == IrTy::I8).collect();
    if !clean_params {
        for r in f.param_regs.iter().flatten() {
            c[r.0 as usize] = false;
        }
    }
    let op = |o: &Operand, c: &[bool]| match o {
        Operand::Imm(v) => (v & 0xffff) < 0x100,
        Operand::Reg(r) => c[r.0 as usize],
        _ => false,
    };
    loop {
        let mut changed = false;
        for b in &f.blocks {
            for i in &b.insts {
                let Some(d) = i.def() else { continue };
                if !c[d.0 as usize] {
                    continue;
                }
                let ok = match i {
                    Inst::Mov { src, .. } => op(src, &c),
                    Inst::Cmp { .. } => true,
                    Inst::Bin { op: BinOp::And, a, b, .. } => op(a, &c) || op(b, &c),
                    Inst::Bin { op: BinOp::Or | BinOp::Xor, a, b, .. } => op(a, &c) && op(b, &c),
                    Inst::Bin { op: BinOp::ShrU, .. } => true,
                    Inst::Conv { kind: ConvKind::Trunc, .. } => false,
                    _ => false,
                };
                if !ok {
                    c[d.0 as usize] = false;
                    changed = true;
                }
            }
        }
        if !changed {
            return c;
        }
    }
}
