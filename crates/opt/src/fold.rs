//! Constant folding and algebraic identities.

use loomcc_ir::*;

fn bin(op: BinOp, t: IrTy, x: i64, y: i64) -> Option<i64> {
    let (sx, sy) = (t.sext(x), t.sext(y));
    let (ux, uy) = (t.zext(x) as u64, t.zext(y) as u64);
    let bits = t.bits() as i64;
    Some(t.zext(match op {
        BinOp::Add => x.wrapping_add(y),
        BinOp::Sub => x.wrapping_sub(y),
        BinOp::Mul => x.wrapping_mul(y),
        BinOp::DivS if sy != 0 => sx.wrapping_div(sy),
        BinOp::DivU if uy != 0 => (ux / uy) as i64,
        BinOp::RemS if sy != 0 => sx.wrapping_rem(sy),
        BinOp::RemU if uy != 0 => (ux % uy) as i64,
        BinOp::And => x & y,
        BinOp::Or => x | y,
        BinOp::Xor => x ^ y,
        BinOp::Shl => {
            let s = t.zext(y);
            if s >= bits { 0 } else { x << s }
        }
        BinOp::ShrS => {
            let s = t.zext(y);
            if s >= bits { if sx < 0 { -1 } else { 0 } } else { sx >> s }
        }
        BinOp::ShrU => {
            let s = t.zext(y);
            if s >= bits { 0 } else { (ux >> s) as i64 }
        }
        _ => return None,
    }))
}

pub fn fold(f: &mut Func) {
    let tys = f.vregs.clone();
    for b in &mut f.blocks {
        for inst in &mut b.insts {
            let new = match &*inst {
                Inst::Bin { op, dst, a, b } => {
                    let t = tys[dst.0 as usize];
                    if t == IrTy::Ptr {
                        None
                    } else {
                        match (a.imm(), b.imm()) {
                            (Some(x), Some(y)) => bin(*op, t, x, y).map(|v| Inst::Mov { dst: *dst, src: Operand::Imm(v) }),
                            (_, Some(y)) => {
                                let y = t.zext(y);
                                match op {
                                    BinOp::Add | BinOp::Sub | BinOp::Or | BinOp::Xor | BinOp::Shl | BinOp::ShrS | BinOp::ShrU if y == 0 => {
                                        Some(Inst::Mov { dst: *dst, src: a.clone() })
                                    }
                                    BinOp::Mul | BinOp::DivS | BinOp::DivU if y == 1 => Some(Inst::Mov { dst: *dst, src: a.clone() }),
                                    BinOp::Mul | BinOp::And if y == 0 => Some(Inst::Mov { dst: *dst, src: Operand::Imm(0) }),
                                    BinOp::And if y == t.mask() as i64 => Some(Inst::Mov { dst: *dst, src: a.clone() }),
                                    BinOp::RemU if y == 1 => Some(Inst::Mov { dst: *dst, src: Operand::Imm(0) }),
                                    _ => None,
                                }
                            }
                            (Some(x), _) => {
                                let x = t.zext(x);
                                match op {
                                    BinOp::Add | BinOp::Or | BinOp::Xor if x == 0 => Some(Inst::Mov { dst: *dst, src: b.clone() }),
                                    BinOp::Mul if x == 1 => Some(Inst::Mov { dst: *dst, src: b.clone() }),
                                    BinOp::Mul | BinOp::And | BinOp::Shl | BinOp::ShrU if x == 0 => Some(Inst::Mov { dst: *dst, src: Operand::Imm(0) }),
                                    _ => None,
                                }
                            }
                            _ => None,
                        }
                    }
                }
                Inst::Un { op, dst, a: Operand::Imm(x) } => {
                    let t = tys[dst.0 as usize];
                    Some(Inst::Mov { dst: *dst, src: Operand::Imm(t.zext(if *op == UnOp::Neg { x.wrapping_neg() } else { !x })) })
                }
                Inst::Cmp { cc, ty, dst, a: Operand::Imm(x), b: Operand::Imm(y) } => {
                    Some(Inst::Mov { dst: *dst, src: Operand::Imm(cc.eval(*ty, *x, *y) as i64) })
                }
                Inst::Conv { kind, dst, src: Operand::Imm(x), from } => {
                    let t = tys[dst.0 as usize];
                    let v = match kind {
                        ConvKind::Sext => from.sext(*x),
                        _ => from.zext(*x),
                    };
                    Some(Inst::Mov { dst: *dst, src: Operand::Imm(t.zext(v)) })
                }
                _ => None,
            };
            if let Some(n) = new {
                *inst = n;
            }
        }
    }
}
