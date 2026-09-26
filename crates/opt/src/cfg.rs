//! CFG clean-up: constant branches, jump threading, block merging,
//! unreachable-block removal (with renumbering; block 0 stays the entry).

use loomcc_ir::*;

pub fn simplify(f: &mut Func) {
    loop {
        let mut changed = false;
        changed |= fold_branches(f);
        changed |= thread_jumps(f);
        changed |= merge_blocks(f);
        changed |= remove_unreachable(f);
        if !changed {
            break;
        }
    }
}

fn fold_branches(f: &mut Func) -> bool {
    let mut changed = false;
    for b in &mut f.blocks {
        let new = match &b.term {
            Term::Br { cond: Operand::Imm(v), t, f: fb } => Some(Term::Jmp(if *v != 0 { *t } else { *fb })),
            Term::Br { cond: Operand::Global(..) | Operand::Slot(..), t, .. } => Some(Term::Jmp(*t)),
            Term::BrCmp { cc, ty, a: Operand::Imm(x), b: Operand::Imm(y), t, f: fb } => {
                Some(Term::Jmp(if cc.eval(*ty, *x, *y) { *t } else { *fb }))
            }
            Term::BrCmp { t, f: fb, .. } if t == fb => Some(Term::Jmp(*t)),
            Term::Br { t, f: fb, .. } if t == fb => Some(Term::Jmp(*t)),
            Term::Switch { val: Operand::Imm(v), ty, cases, default } => {
                let x = ty.zext(*v);
                Some(Term::Jmp(cases.iter().find(|c| ty.zext(c.0) == x).map(|c| c.1).unwrap_or(*default)))
            }
            Term::Switch { cases, default, .. } if cases.iter().all(|c| c.1 == *default) => Some(Term::Jmp(*default)),
            _ => None,
        };
        if let Some(t) = new {
            b.term = t;
            changed = true;
        }
    }
    changed
}

/// Where a jump to `b` really goes (through empty blocks that only jump).
fn final_target(f: &Func, mut b: BlockId) -> BlockId {
    let mut steps = 0;
    while steps < 64 {
        let blk = &f.blocks[b.0 as usize];
        match blk.term {
            Term::Jmp(n) if blk.insts.is_empty() && n != b => b = n,
            _ => break,
        }
        steps += 1;
    }
    b
}

fn thread_jumps(f: &mut Func) -> bool {
    let mut changed = false;
    for i in 0..f.blocks.len() {
        let succs: Vec<BlockId> = f.blocks[i].term.succs();
        let targets: Vec<BlockId> = succs.iter().map(|&s| final_target(f, s)).collect();
        if succs != targets {
            let mut k = 0;
            for s in f.blocks[i].term.succs_mut() {
                *s = targets[k];
                k += 1;
            }
            changed = true;
        }
        // An empty block ending in a return or branch can be copied into a
        // predecessor that jumps to it (tail duplication of tiny blocks).
        if let Term::Jmp(t) = f.blocks[i].term {
            let tb = &f.blocks[t.0 as usize];
            if tb.insts.is_empty() && t.0 as usize != i {
                if let Term::Ret(_) | Term::BrCmp { .. } | Term::Br { .. } = tb.term {
                    let nt = tb.term.clone();
                    f.blocks[i].term = nt;
                    changed = true;
                }
            }
        }
    }
    changed
}

fn merge_blocks(f: &mut Func) -> bool {
    let preds = f.preds();
    let mut changed = false;
    for i in 0..f.blocks.len() {
        if let Term::Jmp(t) = f.blocks[i].term {
            let t = t.0 as usize;
            if t != i && t != 0 && preds[t].len() == 1 && !f.blocks[t].insts.is_empty() {
                let moved = std::mem::replace(&mut f.blocks[t], Block { insts: vec![], term: Term::Unreachable });
                f.blocks[i].insts.extend(moved.insts);
                f.blocks[i].term = moved.term;
                changed = true;
                // Preds changed; stop and let the caller iterate.
                return changed;
            }
        }
    }
    changed
}

pub fn remove_unreachable(f: &mut Func) -> bool {
    let n = f.blocks.len();
    let mut seen = vec![false; n];
    let mut stack = vec![0usize];
    while let Some(b) = stack.pop() {
        if seen[b] {
            continue;
        }
        seen[b] = true;
        for s in f.blocks[b].term.succs() {
            stack.push(s.0 as usize);
        }
    }
    if seen.iter().all(|&s| s) {
        return false;
    }
    let mut map = vec![u32::MAX; n];
    let mut blocks = Vec::new();
    for (i, b) in f.blocks.iter().enumerate() {
        if seen[i] {
            map[i] = blocks.len() as u32;
            blocks.push(b.clone());
        }
    }
    for b in &mut blocks {
        for s in b.term.succs_mut() {
            *s = BlockId(map[s.0 as usize]);
        }
    }
    f.blocks = blocks;
    true
}
