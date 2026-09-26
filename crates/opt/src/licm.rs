//! Loops: invariant code motion and induction-variable strength reduction.

use crate::copy::{dominates, dominators};
use loomcc_ir::*;
use std::collections::BTreeSet;

pub struct Loop {
    pub header: usize,
    pub body: BTreeSet<usize>,
}

/// Natural loops (one per header; bodies of back edges merged).
pub fn find_loops(f: &Func) -> Vec<Loop> {
    let idom = dominators(f);
    let preds = f.preds();
    let mut loops: Vec<Loop> = Vec::new();
    for (t, b) in f.blocks.iter().enumerate() {
        if idom[t].is_none() {
            continue;
        }
        for h in b.term.succs() {
            let h = h.0 as usize;
            if !dominates(&idom, h, t) {
                continue;
            }
            let mut body = BTreeSet::new();
            body.insert(h);
            let mut stack = vec![t];
            while let Some(x) = stack.pop() {
                if body.insert(x) {
                    for p in &preds[x] {
                        stack.push(p.0 as usize);
                    }
                }
            }
            match loops.iter_mut().find(|l| l.header == h) {
                Some(l) => l.body.extend(body),
                None => loops.push(Loop { header: h, body }),
            }
        }
    }
    // Inner loops first.
    loops.sort_by_key(|l| l.body.len());
    loops
}

/// Returns a preheader: the single block outside the loop that jumps to the
/// header (created when needed).
fn preheader(f: &mut Func, l: &Loop) -> Option<usize> {
    let preds = f.preds();
    let outside: Vec<usize> = preds[l.header].iter().map(|p| p.0 as usize).filter(|p| !l.body.contains(p)).collect();
    if outside.is_empty() || l.header == 0 {
        return None;
    }
    if outside.len() == 1 {
        let p = outside[0];
        if matches!(f.blocks[p].term, Term::Jmp(_)) {
            return Some(p);
        }
    }
    let ph = f.blocks.len();
    f.blocks.push(Block { insts: vec![], term: Term::Jmp(BlockId(l.header as u32)) });
    for p in outside {
        for s in f.blocks[p].term.succs_mut() {
            if s.0 as usize == l.header {
                *s = BlockId(ph as u32);
            }
        }
    }
    Some(ph)
}

fn def_counts(f: &Func) -> Vec<u32> {
    let mut c = vec![0u32; f.vregs.len()];
    for b in &f.blocks {
        for i in &b.insts {
            if let Some(d) = i.def() {
                c[d.0 as usize] += 1;
            }
        }
    }
    for r in f.param_regs.iter().flatten() {
        c[r.0 as usize] += 1;
    }
    if let Some(r) = f.sret_reg {
        c[r.0 as usize] += 1;
    }
    c
}

/// Could a store/call in the loop change what `addr` reads?
fn may_be_written(f: &Func, l: &Loop, addr: &Addr) -> bool {
    for &b in &l.body {
        for i in &f.blocks[b].insts {
            let w = match i {
                Inst::Call { .. } => return true,
                Inst::Store { addr: a, ty, .. } => Some((a, ty.size())),
                Inst::Memcpy { dst, size, .. } | Inst::Memset { dst, size, .. } => Some((dst, *size)),
                _ => None,
            };
            if let Some((a, _)) = w {
                let disjoint = match (&a.base, &addr.base) {
                    (Base::Global(x), Base::Global(y)) => x != y,
                    (Base::Slot(x), Base::Slot(y)) => x != y,
                    (Base::Global(_), Base::Slot(_)) | (Base::Slot(_), Base::Global(_)) => true,
                    _ => false,
                };
                if !disjoint {
                    return true;
                }
            }
        }
    }
    false
}

pub fn hoist_invariants(f: &mut Func) {
    hoist_invariants_again(f)
}

fn hoist_invariants_again(f: &mut Func) {
    // Bounded re-run for nested loops (each call hoists one loop's worth).
    for _ in 0..8 {
        let before = f.blocks.iter().map(|b| b.insts.len()).collect::<Vec<_>>();
        let loops = find_loops(f);
        let mut any = false;
        for l in &loops {
            if try_hoist_one(f, l) {
                any = true;
                break;
            }
        }
        let after = f.blocks.iter().map(|b| b.insts.len()).collect::<Vec<_>>();
        if !any || before == after && !any {
            break;
        }
    }
}

fn try_hoist_one(f: &mut Func, l: &Loop) -> bool {
    let defs = def_counts(f);
    let mut defined_in = vec![false; f.vregs.len()];
    for &b in &l.body {
        for i in &f.blocks[b].insts {
            if let Some(d) = i.def() {
                defined_in[d.0 as usize] = true;
            }
        }
    }
    let mut moved = Vec::new();
    for &b in &l.body {
        let mut k = 0;
        while k < f.blocks[b].insts.len() {
            let inst = &f.blocks[b].insts[k];
            let Some(d) = inst.def() else {
                k += 1;
                continue;
            };
            let pure = match inst {
                Inst::Mov { .. } | Inst::Bin { .. } | Inst::Un { .. } | Inst::Cmp { .. } | Inst::Conv { .. } | Inst::Lea { .. } => true,
                Inst::Load { addr, volatile: false, .. } => !may_be_written(f, l, addr),
                _ => false,
            };
            let risky = matches!(inst, Inst::Bin { op: BinOp::DivS | BinOp::DivU | BinOp::RemS | BinOp::RemU, .. });
            if pure && !risky && defs[d.0 as usize] == 1 && inst.uses().iter().all(|u| !defined_in[u.0 as usize]) {
                moved.push(f.blocks[b].insts.remove(k));
                defined_in[d.0 as usize] = false;
                continue;
            }
            k += 1;
        }
    }
    if moved.is_empty() {
        return false;
    }
    match preheader(f, l) {
        Some(ph) => f.blocks[ph].insts.extend(moved),
        None => {
            let h = l.header;
            let mut v = moved;
            v.extend(std::mem::take(&mut f.blocks[h].insts));
            f.blocks[h].insts = v;
        }
    }
    true
}

/// Basic induction variables (`i = i +/- c` once in the loop, 16-bit) make
/// `i*K` and scaled indexes `[b + i*s]` into variables stepped by `c*K`.
pub fn strength_reduce(f: &mut Func) {
    for _ in 0..8 {
        let loops = find_loops(f);
        let mut did = false;
        for l in &loops {
            if reduce_one(f, l) {
                did = true;
                break;
            }
        }
        if !did {
            break;
        }
    }
}

fn reduce_one(f: &mut Func, l: &Loop) -> bool {
    // Find a basic IV.
    let mut defined_in = vec![0u32; f.vregs.len()];
    for &b in &l.body {
        for i in &f.blocks[b].insts {
            if let Some(d) = i.def() {
                defined_in[d.0 as usize] += 1;
            }
        }
    }
    let mut ivs: Vec<(VReg, i64, usize, usize)> = Vec::new(); // (iv, step, block, index)
    for &b in &l.body {
        for (k, i) in f.blocks[b].insts.iter().enumerate() {
            if let Inst::Bin { op: op @ (BinOp::Add | BinOp::Sub), dst, a: Operand::Reg(x), b: Operand::Imm(c) } = i {
                if dst == x && defined_in[dst.0 as usize] == 1 && f.ty(*dst) == IrTy::I16 {
                    let step = if *op == BinOp::Add { *c } else { -*c };
                    ivs.push((*dst, IrTy::I16.sext(step), b, k));
                }
            }
        }
    }
    for (iv, step, ub, uk) in ivs {
        let invariant = |o: &Operand| match o {
            Operand::Imm(_) => true,
            Operand::Reg(r) => defined_in[r.0 as usize] == 0,
            _ => false,
        };
        // Candidates: mul by an invariant, shl by a constant, scaled index.
        // Multiplying by a small power of two is a shift or two: cheaper than
        // another loop variable.
        let cheap = |m: &Operand| matches!(m, Operand::Imm(v) if { let v = v & 0xffff; v != 0 && v & (v - 1) == 0 && v <= 8 });
        let mut factor: Option<Operand> = None;
        'scan: for &b in &l.body {
            for i in &f.blocks[b].insts {
                match i {
                    Inst::Bin { op: BinOp::Mul, a, b: m, .. } if *a == Operand::Reg(iv) && invariant(m) && !cheap(m) => {
                        factor = Some(m.clone());
                        break 'scan;
                    }
                    Inst::Bin { op: BinOp::Mul, a: m, b, .. } if *b == Operand::Reg(iv) && invariant(m) && !cheap(m) => {
                        factor = Some(m.clone());
                        break 'scan;
                    }
                    Inst::Load { addr, .. } | Inst::Store { addr, .. } => {
                        if let Some((r, s)) = addr.index {
                            if r == iv && s > 1 {
                                factor = Some(Operand::Imm(s as i64));
                                break 'scan;
                            }
                        }
                    }
                    _ => {}
                }
            }
        }
        let Some(k) = factor else { continue };
        let Some(ph) = preheader(f, l) else { continue };
        let j = f.new_vreg(IrTy::I16);
        f.blocks[ph].insts.push(Inst::Bin { op: BinOp::Mul, dst: j, a: Operand::Reg(iv), b: k.clone() });
        // The step: c*K (constant, or computed in the preheader).
        let stepop = match &k {
            Operand::Imm(kv) => Operand::Imm(IrTy::I16.zext(step.wrapping_mul(*kv))),
            other => {
                let s = f.new_vreg(IrTy::I16);
                f.blocks[ph].insts.push(Inst::Bin { op: BinOp::Mul, dst: s, a: other.clone(), b: Operand::Imm(IrTy::I16.zext(step)) });
                Operand::Reg(s)
            }
        };
        f.blocks[ub].insts.insert(uk + 1, Inst::Bin { op: BinOp::Add, dst: j, a: Operand::Reg(j), b: stepop });
        // Rewrite the uses.
        let body: Vec<usize> = l.body.iter().copied().collect();
        for b in body {
            for i in &mut f.blocks[b].insts {
                match i {
                    Inst::Bin { op: BinOp::Mul, dst, a, b: m } if (*a == Operand::Reg(iv) && *m == k) || (*m == Operand::Reg(iv) && *a == k) => {
                        *i = Inst::Mov { dst: *dst, src: Operand::Reg(j) };
                    }
                    Inst::Load { addr, .. } | Inst::Store { addr, .. } | Inst::Lea { addr, .. } => {
                        if let (Some((r, s)), Operand::Imm(kv)) = (addr.index, &k) {
                            if r == iv && s as i64 == *kv {
                                addr.index = Some((j, 1));
                            }
                        }
                    }
                    _ => {}
                }
            }
        }
        return true;
    }
    false
}
