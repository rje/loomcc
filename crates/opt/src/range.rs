//! Non-negativity analysis and re-folding of signed indexes.
//!
//! Lowering computes `p + i*s` with 16-bit arithmetic (a `lea`) when `i` is
//! signed, because the 65816's indexed modes carry into the bank byte and a
//! negative index must wrap inside the bank instead. When `i` is provably
//! non-negative the two agree, and the access can index directly
//! (`[p],y`). Arithmetic on registers of a signed C type cannot overflow
//! (it would be undefined), so a sum of non-negative signed values stays
//! non-negative.

use loomcc_ir::*;

/// Registers whose value is always in 0..0x8000 (as 16 bits).
pub fn nonneg(f: &Func) -> Vec<bool> {
    let n = f.vregs.len();
    let mut nn: Vec<bool> = (0..n).map(|i| matches!(f.vregs[i], IrTy::I16 | IrTy::I8)).collect();
    for r in f.param_regs.iter().flatten() {
        nn[r.0 as usize] = false;
    }
    let op_nn = |o: &Operand, nn: &[bool]| match o {
        Operand::Imm(v) => (v & 0xffff) < 0x8000,
        Operand::Reg(r) => nn[r.0 as usize],
        _ => false,
    };
    loop {
        let mut changed = false;
        for b in &f.blocks {
            for i in &b.insts {
                let Some(d) = i.def() else { continue };
                if !nn[d.0 as usize] {
                    continue;
                }
                let ok = match i {
                    Inst::Mov { src, .. } => op_nn(src, &nn),
                    Inst::Conv { kind: ConvKind::Zext, from: IrTy::I8, .. } => true,
                    Inst::Cmp { .. } => true,
                    Inst::Bin { op, dst, a, b } => match op {
                        BinOp::And => op_nn(a, &nn) || op_nn(b, &nn),
                        BinOp::ShrU => b.imm().map_or(false, |k| k & 0xffff >= 1),
                        BinOp::RemU | BinOp::RemS => op_nn(b, &nn) && op_nn(a, &nn),
                        BinOp::DivU => b.imm().map_or(false, |k| k & 0xffff >= 2) || op_nn(a, &nn),
                        BinOp::DivS | BinOp::ShrS => op_nn(a, &nn) && op_nn(b, &nn),
                        BinOp::Add | BinOp::Mul | BinOp::Shl | BinOp::Or => f.is_signed(*dst) && op_nn(a, &nn) && op_nn(b, &nn),
                        _ => false,
                    },
                    // A byte widened later is non-negative; the byte itself
                    // is only used through the zext.
                    Inst::Load { dst, .. } => f.ty(*dst) == IrTy::I8 && false,
                    _ => false,
                };
                if !ok {
                    nn[d.0 as usize] = false;
                    changed = true;
                }
            }
        }
        if !changed {
            return nn;
        }
    }
}

/// `%q = lea [%p + %i*s]; ... load [%q + k]` with %i non-negative becomes
/// `load [%p + k + %i*s]` (same block, %p and %i not redefined between).
pub fn refold_signed_indexes(f: &mut Func) {
    let nn = nonneg(f);
    for bi in 0..f.blocks.len() {
        let n = f.blocks[bi].insts.len();
        for k in 0..n {
            let (q, a) = match &f.blocks[bi].insts[k] {
                Inst::Lea { dst, addr } => match addr.index {
                    Some((i, _)) if nn[i.0 as usize] && matches!(addr.base, Base::Reg(_) | Base::Global(_) | Base::Slot(_)) => (*dst, addr.clone()),
                    _ => continue,
                },
                _ => continue,
            };
            // A scaled index recomputed at every access costs more than one
            // shared pointer; refold those only when used once.
            let uses = f.blocks.iter().flat_map(|b| b.insts.iter().map(|i| i.uses()).chain(std::iter::once(b.term.uses()))).flatten().filter(|u| *u == q).count();
            if a.index.map_or(false, |(_, s)| s != 1) && uses > 1 {
                continue;
            }
            let regs = a.regs();
            for j in k + 1..n {
                let inst = &mut f.blocks[bi].insts[j];
                let mut fix = |ad: &mut Addr| {
                    if ad.base == Base::Reg(q) && ad.index.is_none() {
                        let mut na = a.clone();
                        na.offset += ad.offset;
                        *ad = na;
                    }
                };
                match inst {
                    Inst::Load { addr, .. } | Inst::Store { addr, .. } => fix(addr),
                    _ => {}
                }
                if let Some(d) = inst.def() {
                    if regs.contains(&d) || d == q {
                        break;
                    }
                }
            }
        }
    }
}
