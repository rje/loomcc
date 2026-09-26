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

use loomcc_ir::*;

#[derive(Clone, Debug)]
pub struct Options {
    pub level: u8,
    pub inline: bool,
}

impl Default for Options {
    fn default() -> Options {
        Options { level: 2, inline: true }
    }
}

/// Per-function pipeline, iterated to a fixed point (bounded).
pub fn optimize_func(f: &mut Func, level: u8) {
    if level == 0 {
        cfg::remove_unreachable(f);
        return;
    }
    for _ in 0..6 {
        let before = f.clone();
        cfg::simplify(f);
        fold::fold(f);
        copy::propagate(f);
        copy::retarget_defs(f);
        copy::propagate_local(f);
        copy::retarget_defs(f);
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
    if opts.level >= 2 && opts.inline {
        inline::inline_module(m);
    }
    for f in &mut m.funcs {
        optimize_func(f, opts.level);
    }
    if opts.level >= 1 {
        inline::remove_dead_functions(m);
    }
}

#[cfg(test)]
mod tests;
