//! Inlining (whole program: callees from any unit) and dead-function
//! removal.

use loomcc_ir::*;
use std::collections::{HashMap, HashSet};

/// Always inline callees at most this many IR instructions long.
const SMALL: usize = 16;
/// A function with a single call site in the program (and no other use) is
/// inlined up to this size.
const SINGLE_CALL: usize = 400;
/// Stop growing a caller past this many instructions.
const CALLER_LIMIT: usize = 2000;

fn size(f: &Func) -> usize {
    f.blocks.iter().map(|b| b.insts.len() + 1).sum()
}

fn call_sites(m: &Module) -> HashMap<String, usize> {
    let mut c = HashMap::new();
    for f in &m.funcs {
        for b in &f.blocks {
            for i in &b.insts {
                if let Inst::Call { callee: Callee::Direct(n), .. } = i {
                    *c.entry(n.clone()).or_insert(0) += 1;
                }
            }
        }
    }
    c
}

/// Functions on a call-graph cycle (never inlined).
fn recursive(m: &Module) -> HashSet<String> {
    let names: HashMap<&str, usize> = m.funcs.iter().enumerate().map(|(i, f)| (f.name.as_str(), i)).collect();
    let succ: Vec<Vec<usize>> = m
        .funcs
        .iter()
        .map(|f| {
            let mut v = Vec::new();
            for b in &f.blocks {
                for i in &b.insts {
                    if let Inst::Call { callee: Callee::Direct(n), .. } = i {
                        if let Some(&j) = names.get(n.as_str()) {
                            v.push(j);
                        }
                    }
                }
            }
            v
        })
        .collect();
    let mut rec = HashSet::new();
    for start in 0..m.funcs.len() {
        // Can start reach itself?
        let mut seen = vec![false; m.funcs.len()];
        let mut stack = succ[start].clone();
        while let Some(v) = stack.pop() {
            if v == start {
                rec.insert(m.funcs[start].name.clone());
                break;
            }
            if !seen[v] {
                seen[v] = true;
                stack.extend(succ[v].iter().copied());
            }
        }
    }
    rec
}

pub fn inline_module(m: &mut Module) {
    let rec = recursive(m);
    for _round in 0..4 {
        let sites = call_sites(m);
        let snapshot: HashMap<String, Func> = m.funcs.iter().map(|f| (f.name.clone(), f.clone())).collect();
        let mut changed = false;
        for fi in 0..m.funcs.len() {
            let caller_name = m.funcs[fi].name.clone();
            loop {
                let f = &m.funcs[fi];
                if size(f) > CALLER_LIMIT {
                    break;
                }
                // Find a call to inline.
                let mut pick = None;
                'find: for (bi, b) in f.blocks.iter().enumerate() {
                    for (k, i) in b.insts.iter().enumerate() {
                        if let Inst::Call { callee: Callee::Direct(n), .. } = i {
                            if *n == caller_name || rec.contains(n) {
                                continue;
                            }
                            let Some(g) = snapshot.get(n) else { continue };
                            if g.variadic || g.interrupt != f.interrupt && g.interrupt {
                                continue;
                            }
                            let single = sites.get(n).copied().unwrap_or(0) == 1 && !g.exported && !g.address_taken;
                            let sz = size(g);
                            if sz <= SMALL || (single && sz <= SINGLE_CALL) {
                                pick = Some((bi, k, g.clone()));
                                break 'find;
                            }
                        }
                    }
                }
                let Some((bi, k, g)) = pick else { break };
                inline_call(&mut m.funcs[fi], bi, k, &g);
                changed = true;
            }
        }
        if !changed {
            break;
        }
    }
}

/// Replaces the call at `f.blocks[bi].insts[k]` with a copy of `g`.
pub fn inline_call(f: &mut Func, bi: usize, k: usize, g: &Func) {
    let call = f.blocks[bi].insts[k].clone();
    let Inst::Call { dst, args, sret, .. } = call else { return };
    // Registers, slots and blocks of the copy.
    let rbase = f.vregs.len() as u32;
    for (i, t) in g.vregs.iter().enumerate() {
        f.vregs.push(*t);
        f.signed.push(g.signed.get(i).copied().unwrap_or(false));
    }
    let sbase = f.slots.len() as u32;
    for s in &g.slots {
        f.slots.push(Slot { size: s.size, align: s.align, name: format!("{}.{}", g.name, s.name) });
    }
    let r = |v: VReg| VReg(v.0 + rbase);
    let sl = |s: SlotId| SlotId(s.0 + sbase);
    // Split the caller block.
    let tail: Vec<Inst> = f.blocks[bi].insts.split_off(k + 1);
    f.blocks[bi].insts.pop(); // the call
    let cont_term = std::mem::replace(&mut f.blocks[bi].term, Term::Unreachable);
    let cont = BlockId(f.blocks.len() as u32);
    f.blocks.push(Block { insts: tail, term: cont_term });
    let bbase = f.blocks.len() as u32;
    // Parameter passing.
    let mut pre = Vec::new();
    for (i, p) in g.params.iter().enumerate() {
        let Some(a) = args.get(i) else { break };
        match p {
            ParamKind::Scalar(_) => {
                if let Some(pr) = g.param_regs[i] {
                    pre.push(Inst::Mov { dst: r(pr), src: a.clone() });
                }
            }
            ParamKind::Aggregate(n) => {
                if let Some(ps) = g.param_slots[i] {
                    let src = match a {
                        Operand::Reg(x) => Addr { base: Base::Reg(*x), offset: 0, index: None },
                        Operand::Global(gl, o) => Addr { base: Base::Global(gl.clone()), offset: *o, index: None },
                        Operand::Slot(s, o) => Addr { base: Base::Slot(*s), offset: *o, index: None },
                        Operand::Imm(v) => Addr { base: Base::Abs(*v as u32), offset: 0, index: None },
                    };
                    pre.push(Inst::Memcpy { dst: Addr { base: Base::Slot(sl(ps)), offset: 0, index: None }, src, size: *n });
                }
            }
        }
    }
    if let (Some(sr), Some(sa)) = (g.sret_reg, sret) {
        pre.push(Inst::Lea { dst: r(sr), addr: sa });
    }
    f.blocks[bi].insts.extend(pre);
    f.blocks[bi].term = Term::Jmp(BlockId(bbase));
    // Copy the body.
    let map_addr = |a: &Addr| -> Addr {
        Addr {
            base: match &a.base {
                Base::Reg(x) => Base::Reg(r(*x)),
                Base::Slot(s) => Base::Slot(sl(*s)),
                o => o.clone(),
            },
            offset: a.offset,
            index: a.index.map(|(x, s)| (r(x), s)),
        }
    };
    let map_op = |o: &Operand| -> Operand {
        match o {
            Operand::Reg(x) => Operand::Reg(r(*x)),
            Operand::Slot(s, off) => Operand::Slot(sl(*s), *off),
            o => o.clone(),
        }
    };
    for b in &g.blocks {
        let mut insts = Vec::new();
        for i in &b.insts {
            insts.push(match i {
                Inst::Mov { dst, src } => Inst::Mov { dst: r(*dst), src: map_op(src) },
                Inst::Bin { op, dst, a, b } => Inst::Bin { op: *op, dst: r(*dst), a: map_op(a), b: map_op(b) },
                Inst::Un { op, dst, a } => Inst::Un { op: *op, dst: r(*dst), a: map_op(a) },
                Inst::Cmp { cc, ty, dst, a, b } => Inst::Cmp { cc: *cc, ty: *ty, dst: r(*dst), a: map_op(a), b: map_op(b) },
                Inst::Conv { kind, dst, src, from } => Inst::Conv { kind: *kind, dst: r(*dst), src: map_op(src), from: *from },
                Inst::Load { dst, addr, volatile } => Inst::Load { dst: r(*dst), addr: map_addr(addr), volatile: *volatile },
                Inst::Store { addr, src, ty, volatile } => Inst::Store { addr: map_addr(addr), src: map_op(src), ty: *ty, volatile: *volatile },
                Inst::Lea { dst, addr } => Inst::Lea { dst: r(*dst), addr: map_addr(addr) },
                Inst::Call { dst, callee, args, arg_tys, sret } => Inst::Call {
                    dst: dst.map(r),
                    callee: match callee {
                        Callee::Direct(n) => Callee::Direct(n.clone()),
                        Callee::Indirect(o) => Callee::Indirect(map_op(o)),
                    },
                    args: args.iter().map(map_op).collect(),
                    arg_tys: arg_tys.clone(),
                    sret: sret.as_ref().map(map_addr),
                },
                Inst::Memcpy { dst, src, size } => Inst::Memcpy { dst: map_addr(dst), src: map_addr(src), size: *size },
                Inst::Memset { dst, val, size } => Inst::Memset { dst: map_addr(dst), val: *val, size: *size },
            });
        }
        let bl = |x: BlockId| BlockId(x.0 + bbase);
        let term = match &b.term {
            Term::Jmp(t) => Term::Jmp(bl(*t)),
            Term::Br { cond, t, f: fb } => Term::Br { cond: map_op(cond), t: bl(*t), f: bl(*fb) },
            Term::BrCmp { cc, ty, a, b: bo, t, f: fb } => Term::BrCmp { cc: *cc, ty: *ty, a: map_op(a), b: map_op(bo), t: bl(*t), f: bl(*fb) },
            Term::Switch { val, ty, cases, default } => Term::Switch { val: map_op(val), ty: *ty, cases: cases.iter().map(|(v, t)| (*v, bl(*t))).collect(), default: bl(*default) },
            Term::Ret(v) => {
                if let (Some(d), Some(v)) = (dst, v) {
                    insts.push(Inst::Mov { dst: d, src: map_op(v) });
                }
                Term::Jmp(cont)
            }
            Term::Unreachable => Term::Unreachable,
        };
        f.blocks.push(Block { insts, term });
    }
}

/// Drops non-exported functions nothing references.
pub fn remove_dead_functions(m: &mut Module) {
    loop {
        let mut referenced = HashSet::new();
        for f in &m.funcs {
            for b in &f.blocks {
                let mut direct: Vec<String> = Vec::new();
                let mut globals: Vec<String> = Vec::new();
                let mut refs = |o: &Operand| {
                    if let Operand::Global(g, _) = o {
                        globals.push(g.clone());
                    }
                };
                for i in &b.insts {
                    if let Inst::Call { callee: Callee::Direct(n), .. } = i {
                        direct.push(n.clone());
                    }
                    match i {
                        Inst::Mov { src, .. } => refs(src),
                        Inst::Bin { a, b, .. } | Inst::Cmp { a, b, .. } => {
                            refs(a);
                            refs(b);
                        }
                        Inst::Un { a, .. } => refs(a),
                        Inst::Conv { src, .. } => refs(src),
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
                match &b.term {
                    Term::Ret(Some(o)) | Term::Br { cond: o, .. } | Term::Switch { val: o, .. } => refs(o),
                    Term::BrCmp { a, b, .. } => {
                        refs(a);
                        refs(b);
                    }
                    _ => {}
                }
                referenced.extend(direct);
                referenced.extend(globals);
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
