//! Inlining and dead-function removal (M7).

use loomcc_ir::*;

pub fn inline_module(_m: &mut Module) {}

/// Drops non-exported functions nothing references.
pub fn remove_dead_functions(m: &mut Module) {
    loop {
        let mut referenced = std::collections::HashSet::new();
        for f in &m.funcs {
            for b in &f.blocks {
                for i in &b.insts {
                    if let Inst::Call { callee: Callee::Direct(n), .. } = i {
                        referenced.insert(n.clone());
                    }
                    let mut refs = |o: &Operand| {
                        if let Operand::Global(g, _) = o {
                            referenced.insert(g.clone());
                        }
                    };
                    match i {
                        Inst::Mov { src, .. } => refs(src),
                        Inst::Bin { a, b, .. } | Inst::Cmp { a, b, .. } => {
                            refs(a);
                            refs(b);
                        }
                        Inst::Store { src, .. } => refs(src),
                        Inst::Call { args, callee, .. } => {
                            for a in args {
                                refs(a);
                            }
                            if let Callee::Indirect(o) = callee {
                                refs(o);
                            }
                        }
                        _ => {}
                    }
                }
                if let Term::Ret(Some(Operand::Global(g, _))) = &b.term {
                    referenced.insert(g.clone());
                }
            }
        }
        for g in &m.globals {
            if let Some((_, relocs)) = &g.init {
                for r in relocs {
                    referenced.insert(r.target.clone());
                }
            }
        }
        let before = m.funcs.len();
        m.funcs.retain(|f| f.exported || f.address_taken || f.interrupt || f.name == "main" || referenced.contains(&f.name));
        if m.funcs.len() == before {
            break;
        }
    }
}
