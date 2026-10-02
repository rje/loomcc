//! loomcc-opt: IR optimisation passes.
//!
//! Every pass keeps the IR interpreter's semantics; `tests` runs the IR test
//! programs with and without each pass.

pub mod cfg;
pub mod copy;
pub mod dce;
pub mod fold;
pub mod inline;
pub mod licm;
pub mod range;
pub mod tailrec;
pub mod widen;

use loomcc_ir::*;

#[derive(Clone, Debug)]
pub struct Options {
    pub level: u8,
    pub inline: bool,
    /// Functions nothing may be inlined into (the driver's retry for a
    /// function whose code overflowed a ROM bank).
    pub no_inline_into: std::collections::HashSet<String>,
}

impl Default for Options {
    fn default() -> Options {
        Options { level: 2, inline: true, no_inline_into: Default::default() }
    }
}

/// Functions on a cycle of the call graph: direct calls, and calls through
/// pointers to any function whose address is taken (the back end gives
/// these frames on the hardware stack, whose prologue masks byte
/// parameters).
pub fn recursive_functions(m: &Module) -> std::collections::HashSet<String> {
    use std::collections::HashMap;
    let index: HashMap<&str, usize> = m.funcs.iter().enumerate().map(|(i, f)| (f.name.as_str(), i)).collect();
    let taken: Vec<usize> = m.funcs.iter().enumerate().filter(|(_, f)| f.address_taken).map(|(i, _)| i).collect();
    let succ: Vec<Vec<usize>> = m
        .funcs
        .iter()
        .map(|f| {
            let mut v = Vec::new();
            for b in &f.blocks {
                for i in &b.insts {
                    match i {
                        Inst::Call { callee: Callee::Direct(c), .. } => v.extend(index.get(c.as_str()).copied()),
                        Inst::Call { callee: Callee::Indirect(_), .. } => v.extend(taken.iter().copied()),
                        _ => {}
                    }
                }
            }
            v.sort_unstable();
            v.dedup();
            v
        })
        .collect();
    let mut out = std::collections::HashSet::new();
    for start in 0..m.funcs.len() {
        // Does `start` reach itself?
        let mut seen = vec![false; m.funcs.len()];
        let mut stack = succ[start].clone();
        while let Some(v) = stack.pop() {
            if v == start {
                out.insert(m.funcs[start].name.clone());
                break;
            }
            if !std::mem::replace(&mut seen[v], true) {
                stack.extend(succ[v].iter().copied());
            }
        }
    }
    out
}

/// Per-function pipeline, iterated to a fixed point (bounded).
pub fn optimize_func(f: &mut Func, level: u8) {
    optimize_func_ext(f, level, false)
}

/// `recursive`: the function is on a call-graph cycle (see
/// `recursive_functions`).
pub fn optimize_func_ext(f: &mut Func, level: u8, recursive: bool) {
    if level == 0 {
        cfg::remove_unreachable(f);
        return;
    }
    for _ in 0..6 {
        let before = f.clone();
        tailrec::eliminate(f);
        cfg::simplify(f);
        fold::fold(f);
        copy::propagate(f);
        copy::retarget_defs(f);
        copy::propagate_local(f);
        copy::retarget_defs(f);
        if level >= 2 && widen::widen_and_scale(f, recursive) {
            copy::propagate(f);
            copy::retarget_defs(f);
        }
        if level >= 2 {
            widen::scale_once(f);
            widen::narrow_compares(f);
        }
        range::refold_signed_indexes(f);
        range::unsign_compares(f);
        dce::dce(f);
        if level >= 2 {
            licm::hoist_invariants(f);
            licm::strength_reduce(f);
        }
        cfg::simplify(f);
        if *f == before {
            break;
        }
    }
}

pub fn optimize_module(m: &mut Module, opts: &Options) {
    let recursive = recursive_functions(m);
    for f in &mut m.funcs {
        let r = recursive.contains(&f.name);
        optimize_func_ext(f, opts.level, r);
    }
    if opts.level >= 2 && opts.inline {
        inline::inline_module(m, &opts.no_inline_into);
        let recursive = recursive_functions(m);
        for f in &mut m.funcs {
            let r = recursive.contains(&f.name);
            optimize_func_ext(f, opts.level, r);
        }
    }
    if opts.level >= 1 {
        inline::remove_dead_functions(m);
    }
}

#[cfg(test)]
mod tests;
