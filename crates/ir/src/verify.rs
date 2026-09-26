//! Structural checks on IR functions (every register defined before use on
//! some path is not checked; types of operands are).

use crate::ir::*;

pub fn verify_func(f: &Func) -> Result<(), String> {
    let nb = f.blocks.len() as u32;
    let check_reg = |r: VReg| -> Result<(), String> {
        if (r.0 as usize) < f.vregs.len() {
            Ok(())
        } else {
            Err(format!("{}: register %{} out of range", f.name, r.0))
        }
    };
    for (bi, b) in f.blocks.iter().enumerate() {
        for i in &b.insts {
            for r in i.uses() {
                check_reg(r)?;
            }
            if let Some(d) = i.def() {
                check_reg(d)?;
            }
            match i {
                Inst::Load { addr, .. } | Inst::Store { addr, .. } | Inst::Lea { addr, .. } => {
                    if let Base::Reg(r) = addr.base {
                        if f.ty(r) != IrTy::Ptr {
                            return Err(format!("{} b{}: address base %{} is {} not ptr", f.name, bi, r.0, f.ty(r)));
                        }
                    }
                    if let Some((r, _)) = addr.index {
                        if f.ty(r) == IrTy::Ptr {
                            return Err(format!("{} b{}: address index %{} is a pointer", f.name, bi, r.0));
                        }
                    }
                }
                _ => {}
            }
        }
        for s in b.term.succs() {
            if s.0 >= nb {
                return Err(format!("{} b{}: branch to missing block b{}", f.name, bi, s.0));
            }
        }
    }
    Ok(())
}

pub fn verify_module(m: &Module) -> Result<(), String> {
    for f in &m.funcs {
        verify_func(f)?;
    }
    Ok(())
}
