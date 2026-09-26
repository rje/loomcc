//! Dead code elimination.

use loomcc_ir::*;

pub fn dce(f: &mut Func) {
    loop {
        let mut uses = vec![0u32; f.vregs.len()];
        for b in &f.blocks {
            for i in &b.insts {
                for u in i.uses() {
                    uses[u.0 as usize] += 1;
                }
            }
            for u in b.term.uses() {
                uses[u.0 as usize] += 1;
            }
        }
        let mut changed = false;
        for b in &mut f.blocks {
            let before = b.insts.len();
            b.insts.retain(|i| {
                if i.has_side_effects() {
                    return true;
                }
                match i.def() {
                    Some(d) => uses[d.0 as usize] > 0,
                    None => true,
                }
            });
            // Self-moves.
            b.insts.retain(|i| !matches!(i, Inst::Mov { dst, src: Operand::Reg(s) } if dst == s));
            changed |= b.insts.len() != before;
        }
        if !changed {
            break;
        }
    }
}
