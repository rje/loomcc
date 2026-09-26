//! Copy and constant propagation, and retargeting a temporary's definition
//! onto the variable it is copied into.

use loomcc_ir::*;

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

fn use_counts(f: &Func) -> Vec<u32> {
    let mut c = vec![0u32; f.vregs.len()];
    for b in &f.blocks {
        for i in &b.insts {
            for u in i.uses() {
                c[u.0 as usize] += 1;
            }
        }
        for u in b.term.uses() {
            c[u.0 as usize] += 1;
        }
    }
    c
}

fn replace_all(f: &mut Func, from: VReg, to: &Operand) {
    let mut m = |r: VReg| if r == from { to.clone() } else { Operand::Reg(r) };
    for b in &mut f.blocks {
        for i in &mut b.insts {
            i.map_uses(&mut m);
        }
        b.term.map_uses(&mut m);
    }
}

/// Is `o` usable wherever a register of type `t` is used?
fn fits(f: &Func, o: &Operand, t: IrTy) -> bool {
    match o {
        Operand::Reg(r) => f.ty(*r) == t,
        Operand::Imm(_) => t != IrTy::Ptr,
        Operand::Global(..) | Operand::Slot(..) => t == IrTy::Ptr,
    }
}

pub fn propagate(f: &mut Func) {
    // Global: single-definition copies of constants or of single-definition
    // registers.
    loop {
        let defs = def_counts(f);
        let mut found = None;
        'outer: for b in &f.blocks {
            for i in &b.insts {
                if let Inst::Mov { dst, src } = i {
                    if defs[dst.0 as usize] != 1 {
                        continue;
                    }
                    let t = f.ty(*dst);
                    let ok = match src {
                        Operand::Imm(_) | Operand::Global(..) | Operand::Slot(..) => true,
                        Operand::Reg(s) => defs[s.0 as usize] == 1 && s != dst,
                    };
                    if ok && fits(f, src, t) {
                        let src = match src {
                            Operand::Imm(v) => Operand::Imm(t.zext(*v)),
                            o => o.clone(),
                        };
                        found = Some((*dst, src));
                        break 'outer;
                    }
                }
            }
        }
        match found {
            Some((d, s)) => {
                replace_all(f, d, &s);
                // The Mov itself is now dead (DCE removes it) unless d is a
                // parameter; make it a self-move to be safe.
                for b in &mut f.blocks {
                    b.insts.retain(|i| !matches!(i, Inst::Mov { dst, .. } if *dst == d));
                }
            }
            None => break,
        }
    }
}

/// Within a block, a copy's uses until either side is redefined.
pub fn propagate_local(f: &mut Func) {
    for bi in 0..f.blocks.len() {
        let mut avail: Vec<(VReg, Operand)> = Vec::new();
        let n = f.blocks[bi].insts.len();
        for k in 0..n {
            let tys = f.vregs.clone();
            {
                let inst = &mut f.blocks[bi].insts[k];
                let mut m = |r: VReg| match avail.iter().find(|(d, _)| *d == r) {
                    Some((_, o)) => o.clone(),
                    None => Operand::Reg(r),
                };
                inst.map_uses(&mut m);
            }
            let inst = &f.blocks[bi].insts[k];
            if let Some(d) = inst.def() {
                avail.retain(|(x, o)| *x != d && o.reg() != Some(d));
                if let Inst::Mov { dst, src } = inst {
                    let t = tys[dst.0 as usize];
                    let ok = match src {
                        Operand::Reg(s) => tys[s.0 as usize] == t && s != dst,
                        Operand::Imm(_) => t != IrTy::Ptr,
                        _ => t == IrTy::Ptr,
                    };
                    if ok {
                        let src = match src {
                            Operand::Imm(v) => Operand::Imm(t.zext(*v)),
                            o => o.clone(),
                        };
                        avail.push((*dst, src));
                    }
                }
            }
        }
        let mut term = f.blocks[bi].term.clone();
        let mut m = |r: VReg| match avail.iter().find(|(d, _)| *d == r) {
            Some((_, o)) => o.clone(),
            None => Operand::Reg(r),
        };
        term.map_uses(&mut m);
        f.blocks[bi].term = term;
    }
}

/// `t = op ...; ...; d = t` (t used once) becomes `d = op ...` when nothing
/// in between reads or writes d.
pub fn retarget_defs(f: &mut Func) {
    let uses = use_counts(f);
    let defs = def_counts(f);
    for bi in 0..f.blocks.len() {
        let mut k = 0;
        while k < f.blocks[bi].insts.len() {
            let Some(t) = f.blocks[bi].insts[k].def() else {
                k += 1;
                continue;
            };
            if uses[t.0 as usize] != 1 || defs[t.0 as usize] != 1 || f.param_regs.contains(&Some(t)) {
                k += 1;
                continue;
            }
            // Find the Mov that uses t in this block.
            let mut j = k + 1;
            let mut target = None;
            while j < f.blocks[bi].insts.len() {
                let inst = &f.blocks[bi].insts[j];
                if let Inst::Mov { dst, src: Operand::Reg(s) } = inst {
                    if *s == t {
                        target = Some((j, *dst));
                        break;
                    }
                }
                if inst.uses().contains(&t) {
                    break;
                }
                j += 1;
            }
            let Some((j, d)) = target else {
                k += 1;
                continue;
            };
            if f.ty(d) != f.ty(t) || f.param_regs.contains(&Some(d)) && false {
                k += 1;
                continue;
            }
            let clash = f.blocks[bi].insts[k + 1..j].iter().any(|i| i.uses().contains(&d) || i.def() == Some(d));
            if clash {
                k += 1;
                continue;
            }
            // A call's result register written directly is fine too.
            let inst = &mut f.blocks[bi].insts[k];
            set_def(inst, d);
            f.blocks[bi].insts.remove(j);
            k += 1;
        }
    }
}

fn set_def(i: &mut Inst, d: VReg) {
    match i {
        Inst::Mov { dst, .. }
        | Inst::Bin { dst, .. }
        | Inst::Un { dst, .. }
        | Inst::Cmp { dst, .. }
        | Inst::Conv { dst, .. }
        | Inst::Load { dst, .. }
        | Inst::Lea { dst, .. } => *dst = d,
        Inst::Call { dst, .. } => *dst = Some(d),
        _ => {}
    }
}
