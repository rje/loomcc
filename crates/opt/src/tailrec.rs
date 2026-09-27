//! Tail-recursion elimination: a call of the function itself whose result is
//! returned directly (or a void call followed by `return;`) becomes an
//! assignment of the arguments to the parameters and a jump back to the
//! start of the body.
//!
//! Only for functions whose parameters are all scalars in registers and
//! which have no frame slots: a slot's address could have been passed down
//! the call, and a loop reuses the one frame where recursion would have
//! made a new one.

use loomcc_ir::*;

pub fn eliminate(f: &mut Func) -> bool {
    if f.variadic || f.sret.is_some() || !f.slots.is_empty() || f.param_regs.iter().any(|r| r.is_none()) {
        return false;
    }
    let params: Vec<VReg> = f.param_regs.iter().map(|r| r.unwrap()).collect();
    let is_tail = |b: &Block, name: &str| -> bool {
        let Some(Inst::Call { dst, callee: Callee::Direct(n), args, sret: None, .. }) = b.insts.last() else {
            return false;
        };
        if n != name || args.len() != params.len() {
            return false;
        }
        match (&b.term, dst) {
            (Term::Ret(None), _) => true,
            (Term::Ret(Some(Operand::Reg(r))), Some(d)) => r == d,
            _ => false,
        }
    };
    let tails: Vec<usize> = (0..f.blocks.len()).filter(|&i| is_tail(&f.blocks[i], &f.name)).collect();
    if tails.is_empty() {
        return false;
    }
    // The body moves to a new block, the loop head; the entry jumps to it.
    let head = BlockId(f.blocks.len() as u32);
    let body = std::mem::replace(&mut f.blocks[0], Block { insts: Vec::new(), term: Term::Jmp(head) });
    f.blocks.push(body);
    // Branches back to the old entry now mean the head.
    for b in f.blocks.iter_mut().skip(1) {
        retarget(&mut b.term, BlockId(0), head);
    }
    for i in tails {
        let i = if i == 0 { head.0 as usize } else { i };
        let Some(Inst::Call { args, .. }) = f.blocks[i].insts.pop() else { unreachable!() };
        // A parallel assignment: every argument is read before any
        // parameter is written.
        let mut temps = Vec::new();
        for (k, a) in args.into_iter().enumerate() {
            let t = f.new_vreg(f.ty(params[k]));
            if f.is_signed(params[k]) {
                f.signed[t.0 as usize] = true;
            }
            f.blocks[i].insts.push(Inst::Mov { dst: t, src: a });
            temps.push(t);
        }
        for (k, t) in temps.into_iter().enumerate() {
            f.blocks[i].insts.push(Inst::Mov { dst: params[k], src: Operand::Reg(t) });
        }
        f.blocks[i].term = Term::Jmp(head);
    }
    true
}

fn retarget(t: &mut Term, from: BlockId, to: BlockId) {
    let fix = |b: &mut BlockId| {
        if *b == from {
            *b = to;
        }
    };
    match t {
        Term::Jmp(b) => fix(b),
        Term::Br { t, f, .. } | Term::BrCmp { t, f, .. } => {
            fix(t);
            fix(f);
        }
        Term::Switch { cases, default, .. } => {
            for (_, b) in cases.iter_mut() {
                fix(b);
            }
            fix(default);
        }
        Term::Ret(_) | Term::Unreachable => {}
    }
}
