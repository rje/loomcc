//! Real frames for recursive functions.
//!
//! A function in a recursive call-graph component (outside interrupt
//! context) gets a frame on the hardware stack instead of a static one: its
//! prologue saves D, carves the frame out of the stack and points D at it,
//! so every activation has its own locals, address-taken ones included.
//!
//! Frame layout, from D upwards:
//!
//! ```text
//!   D+$00..base-1     this activation's copy of the direct-page register
//!                     file (the allocator's homes and the scratch words)
//!   D+base..          the function's frame (spilled values and slots)
//!   D+size..+1        the caller's D (phd)
//!   D+size+2..+4      the return address (jsl)
//!   D+size+5..        the arguments, pushed by the caller right to left
//!                     (the 816-tcc layout: 8-bit arguments take one byte)
//! ```
//!
//! Direct-page offsets are 8 bits: spilled values must lie within 256 bytes
//! of D (a function with more keeps a static frame, saved around
//! re-entering calls); slots and arguments further up are reached with
//! `dp,x`, which adds X to D without wrapping.
//!
//! The rest of the program runs with D = 0. Calls from a dynamic-frame
//! function to anything else (static-frame functions, 816-tcc code,
//! PVSnesLib, hand assembly) switch D back to 0 around the `jsl`, and write
//! the callee's direct-page parameter homes with absolute addressing: with
//! DBR = $7E, `$7E:0000-$1FFF` is the same memory as the bank-0 direct page.

use loomcc_ir::*;

/// Bytes of the per-activation direct-page register file at the frame's
/// base: at least the scratch words ($18-$1F, and the allocation pool below
/// them), more when the function's homes use the pool's words above.
pub const MIRROR_MIN: u32 = 0x20;

/// Where a function's frame starts above D: past its register file.
pub fn frame_base(a: &crate::alloc::Alloc) -> u32 {
    a.dp_used.iter().map(|&d| d as u32 + 2).max().unwrap_or(0).max(MIRROR_MIN)
}

/// Bytes the caller pushes for a call's arguments, in the 816-tcc layout.
pub fn arg_bytes(params: &[ParamKind], sret: bool) -> u32 {
    let mut n = if sret { 4 } else { 0 };
    for p in params {
        n += match p {
            ParamKind::Scalar(IrTy::I8) => 1,
            ParamKind::Scalar(IrTy::I16) => 2,
            ParamKind::Scalar(_) => 4,
            ParamKind::Aggregate(n) => *n,
        };
    }
    n
}

/// Whether an allocation suits a frame on the hardware stack: every
/// spilled value and register parameter (the frame's `Frame` homes, which
/// the code generator addresses directly) within direct-page reach. Slots
/// and incoming arguments further up are reached through X (`dp,x` adds X
/// to D without wrapping).
pub fn homes_near(a: &crate::alloc::Alloc, f: &Func) -> bool {
    a.homes.iter().enumerate().all(|(v, h)| match h {
        crate::alloc::Home::Frame(o) => frame_base(a) + o + f.vregs[v].size().max(2) <= 256,
        _ => true,
    })
}

/// Whether some slot may lie beyond direct-page reach, so slot accesses go
/// through X (the allocator must know).
pub fn needs_slot_x(a: &crate::alloc::Alloc) -> bool {
    frame_base(a) + a.frame_size + 4 > 256
}

/// Replaces every frame-slot address used as a value with a register set
/// by a `Lea` just before the use: a dynamic frame's addresses are computed
/// at run time (D plus an offset), so they cannot be immediates.
pub fn materialize_slot_addresses(f: &mut Func) {
    for bi in 0..f.blocks.len() {
        let insts = std::mem::take(&mut f.blocks[bi].insts);
        let mut out = Vec::with_capacity(insts.len());
        for mut inst in insts {
            let mut leas = Vec::new();
            for o in value_operands(&mut inst) {
                if let Operand::Slot(s, off) = *o {
                    let t = f.new_vreg(IrTy::Ptr);
                    leas.push(Inst::Lea { dst: t, addr: Addr { base: Base::Slot(s), offset: off, index: None } });
                    *o = Operand::Reg(t);
                }
            }
            out.extend(leas);
            out.push(inst);
        }
        let mut term = f.blocks[bi].term.clone();
        for o in term_operands(&mut term) {
            if let Operand::Slot(s, off) = *o {
                let t = f.new_vreg(IrTy::Ptr);
                out.push(Inst::Lea { dst: t, addr: Addr { base: Base::Slot(s), offset: off, index: None } });
                *o = Operand::Reg(t);
            }
        }
        f.blocks[bi].term = term;
        f.blocks[bi].insts = out;
    }
}

fn value_operands(i: &mut Inst) -> Vec<&mut Operand> {
    match i {
        Inst::Mov { src, .. } | Inst::Conv { src, .. } | Inst::Store { src, .. } => vec![src],
        Inst::Bin { a, b, .. } | Inst::Cmp { a, b, .. } => vec![a, b],
        Inst::Un { a, .. } => vec![a],
        Inst::Call { callee, args, .. } => {
            let mut v: Vec<&mut Operand> = args.iter_mut().collect();
            if let Callee::Indirect(o) = callee {
                v.push(o);
            }
            v
        }
        Inst::Load { .. } | Inst::Lea { .. } | Inst::Memcpy { .. } | Inst::Memset { .. } => vec![],
    }
}

fn term_operands(t: &mut Term) -> Vec<&mut Operand> {
    match t {
        Term::Br { cond, .. } => vec![cond],
        Term::BrCmp { a, b, .. } => vec![a, b],
        Term::Switch { val, .. } => vec![val],
        Term::Ret(Some(o)) => vec![o],
        _ => vec![],
    }
}
