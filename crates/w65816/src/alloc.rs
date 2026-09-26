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
}

pub struct Alloc {
    pub homes: Vec<Home>,
    /// Kept in A from its definition to its single use in the next
    /// instruction.
    pub forwarded: Vec<bool>,
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

pub fn allocate(f: &Func, dp_allowed: &[u8], call_clobbers_all: bool) -> Alloc {
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

    // 1. Forwarding.
    let mut forwarded = vec![false; n];
    for (bi, b) in f.blocks.iter().enumerate() {
        for (k, inst) in b.insts.iter().enumerate() {
            let Some(d) = inst.def() else { continue };
            if uses[d.0 as usize] != 1 || defs[d.0 as usize] != 1 || is_param[d.0 as usize] || lv.live_out[bi].get(d.0) {
                continue;
            }
            if !defines_in_a(f, inst) {
                continue;
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
    let needs: Vec<bool> = (0..n).map(|i| !forwarded[i] && (uses[i] > 0 || is_param[i])).collect();
    let mut adj: Vec<Vec<u32>> = vec![Vec::new(); n];
    let add_edge = |a: u32, b: u32, adj: &mut Vec<Vec<u32>>| {
        if a != b && !adj[a as usize].contains(&b) {
            adj[a as usize].push(b);
            adj[b as usize].push(a);
        }
    };
    let mut cross_call = vec![false; n];
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
            if matches!(inst, Inst::Call { .. }) && call_clobbers_all {
                for o in after[k].iter() {
                    if Some(VReg(o)) != inst.def() {
                        cross_call[o as usize] = true;
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
    let mut order: Vec<u32> = (0..n as u32).filter(|&i| needs[i as usize]).collect();
    order.sort_by_key(|&i| {
        let b = bases[i as usize] as u64;
        std::cmp::Reverse((b << 40) + score[i as usize])
    });
    let mut homes = vec![Home::None; n];
    let mut dp_used: Vec<u8> = Vec::new();
    for &v in &order {
        let t = f.ty(VReg(v));
        let w = words(t);
        let param_slot = f.param_regs.iter().position(|r| *r == Some(VReg(v)));
        if !cross_call[v as usize] {
            // Try direct page.
            // Precise: a neighbour's words.
            let mut busy: Vec<u8> = Vec::new();
            for &o in &adj[v as usize] {
                if let Home::Dp(d) = homes[o as usize] {
                    busy.push(d);
                    if words(f.ty(VReg(o))) == 2 {
                        busy.push(d + 2);
                    }
                }
            }
            let pick = dp_allowed.iter().copied().find(|&d| {
                let ok1 = !busy.contains(&d);
                if w == 1 {
                    ok1
                } else {
                    ok1 && dp_allowed.contains(&(d + 2)) && !busy.contains(&(d + 2))
                }
            });
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
    // Parameters placed in DP are copied from their slot at entry.
    let mut param_copies = Vec::new();
    for (pi, r) in f.param_regs.iter().enumerate() {
        if let Some(r) = r {
            if let Home::Dp(_) = homes[r.0 as usize] {
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
    Alloc { homes, forwarded, uses, param_offsets, sret_offset, slot_offsets, frame_size, dp_used, param_copies }
}
