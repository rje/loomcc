//! loomcc-w65816: the 65816 backend. Emits WLA-DX assembly that links beside
//! 816-tcc objects: every externally visible function has an 816-tcc ABI
//! entry under its C name; internal calls pass arguments straight into the
//! callee's static frame and return in A (A:X for 32-bit values).

pub mod alloc;
pub mod analysis;
pub mod asm;
pub mod dynframe;
pub mod isel;

use alloc::{allocate, Alloc, DP_POOL};
use asm::{print_line, relax_branches, Line, Mode};
use isel::{Gen, Helper};
use loomcc_ir::*;
use std::collections::{BTreeMap, BTreeSet, HashMap, HashSet};
use std::fmt::Write as _;

#[derive(Clone, Debug)]
pub struct FuncInfo {
    pub frame_sym: String,
    pub body_label: String,
    pub params: Vec<ParamKind>,
    pub param_offsets: Vec<u32>,
    pub sret_offset: Option<u32>,
    pub ret: Option<IrTy>,
    pub has_abi_entry: bool,
    /// Where each scalar parameter lives in the callee (callers write it
    /// there); None for aggregates (their frame slot).
    pub param_homes: Vec<Option<alloc::Home>>,
    pub sret_home: Option<alloc::Home>,
    /// A frame on the hardware stack (recursive functions; see dynframe):
    /// its size in bytes, direct-page register file included. Callers push
    /// the arguments in the 816-tcc layout and jump to the body label.
    pub dyn_frame: Option<u32>,
    /// Its slots may be beyond direct-page reach: slot accesses use X.
    pub dyn_slot_x: bool,
    /// Offset of the frame above D (past the register file).
    pub dyn_base: u32,
}

pub struct ModuleInfo {
    pub funcs: HashMap<String, FuncInfo>,
    /// Globals known to live in bank $7E (16-bit absolute addressing works
    /// with DBR = $7E).
    pub near_globals: HashSet<String>,
    pub tag: String,
    /// cstack offset of each function's frame.
    pub frame_base: HashMap<String, u32>,
    /// Parameter kinds of functions outside the module (816-tcc ABI calls).
    pub externs: HashMap<String, Vec<ParamKind>>,
    /// Functions reachable from an interrupt root.
    pub interrupt_funcs: HashSet<String>,
    /// Recursive call-graph components: function -> component index.
    pub scc_of: HashMap<String, usize>,
    /// Members of each recursive component with their frame sizes.
    pub scc_members: Vec<Vec<(String, u32)>>,
}

impl ModuleInfo {
    /// The function has a frame on the hardware stack (D points at it).
    pub fn is_dyn(&self, f: &str) -> bool {
        self.funcs.get(f).map_or(false, |i| i.dyn_frame.is_some())
    }
}

/// Functions reachable from an interrupt root through direct calls.
fn interrupt_reach(m: &Module) -> HashSet<String> {
    let index: HashMap<&str, usize> = m.funcs.iter().enumerate().map(|(i, f)| (f.name.as_str(), i)).collect();
    let mut out = HashSet::new();
    let mut stack: Vec<usize> = m.funcs.iter().enumerate().filter(|(_, f)| f.interrupt).map(|(i, _)| i).collect();
    while let Some(v) = stack.pop() {
        if out.insert(m.funcs[v].name.clone()) {
            for b in &m.funcs[v].blocks {
                for i in &b.insts {
                    if let Inst::Call { callee: Callee::Direct(n), .. } = i {
                        if let Some(&j) = index.get(n.as_str()) {
                            stack.push(j);
                        }
                    }
                }
            }
        }
    }
    out
}

/// Strongly connected components of the direct call graph that contain a
/// cycle (recursion).
fn recursive_components(m: &Module, callbacks: &HashSet<String>) -> Vec<Vec<usize>> {
    let n = m.funcs.len();
    let index: HashMap<&str, usize> = m.funcs.iter().enumerate().map(|(i, f)| (f.name.as_str(), i)).collect();
    let succ: Vec<Vec<usize>> = m
        .funcs
        .iter()
        .map(|f| {
            let mut v = Vec::new();
            for b in &f.blocks {
                for i in &b.insts {
                    match i {
                        Inst::Call { callee: Callee::Direct(c), .. } => match index.get(c.as_str()) {
                            Some(&j) => v.push(j),
                            // Code outside the module may call back.
                            None if !may_call_back(c) => {}
                            None => v.extend(m.funcs.iter().enumerate().filter(|(_, g)| callbacks.contains(&g.name)).map(|(j, _)| j)),
                        },
                        Inst::Call { callee: Callee::Indirect(_), .. } => {
                            v.extend(m.funcs.iter().enumerate().filter(|(_, g)| g.address_taken).map(|(j, _)| j));
                        }
                        _ => {}
                    }
                }
            }
            v
        })
        .collect();
    // Tarjan.
    struct St {
        idx: Vec<Option<usize>>,
        low: Vec<usize>,
        on: Vec<bool>,
        stack: Vec<usize>,
        next: usize,
        out: Vec<Vec<usize>>,
    }
    fn go(v: usize, succ: &[Vec<usize>], st: &mut St) {
        st.idx[v] = Some(st.next);
        st.low[v] = st.next;
        st.next += 1;
        st.stack.push(v);
        st.on[v] = true;
        for &w in &succ[v] {
            match st.idx[w] {
                None => {
                    go(w, succ, st);
                    st.low[v] = st.low[v].min(st.low[w]);
                }
                Some(i) if st.on[w] => st.low[v] = st.low[v].min(i),
                _ => {}
            }
        }
        if Some(st.low[v]) == st.idx[v] {
            let mut comp = Vec::new();
            loop {
                let w = st.stack.pop().unwrap();
                st.on[w] = false;
                comp.push(w);
                if w == v {
                    break;
                }
            }
            let cyclic = comp.len() > 1 || succ[v].contains(&v);
            if cyclic {
                st.out.push(comp);
            }
        }
    }
    let mut st = St { idx: vec![None; n], low: vec![0; n], on: vec![false; n], stack: Vec::new(), next: 0, out: Vec::new() };
    for v in 0..n {
        if st.idx[v].is_none() {
            go(v, &succ, &mut st);
        }
    }
    st.out
}

impl ModuleInfo {
    pub fn frame_symbol(&self, f: &str) -> String {
        format!("lcf_{}", f)
    }

    pub fn helper_name(&self, h: Helper) -> String {
        let base = match h {
            Helper::Mul16 => "mul16",
            Helper::DivU16 => "divu16",
            Helper::DivS16 => "divs16",
            Helper::JslR10 => "jslr10",
            Helper::Mul16Soft => "mul16s",
            Helper::DivU16Soft => "divu16s",
            Helper::DivS16Soft => "divs16s",
            Helper::A32(c, irq) => {
                let n = ["mul32", "divu32", "divs32", "remu32", "rems32"][c as usize];
                return format!("lcc_{}{}_{}", n, if irq { "i" } else { "" }, self.tag);
            }
        };
        format!("lcc_{}_{}", base, self.tag)
    }
}

#[derive(Clone, Debug)]
pub struct Options {
    /// ROM code bank base (`$80` for LoROM FastROM).
    pub rom_base: u32,
    /// Unit tag for private labels (the output file stem).
    pub tag: String,
    /// Functions that code outside the module (hand assembly) calls back
    /// while one of our functions is active (found by scanning `.asm`
    /// inputs for `jsl`/`jsr.l` targets). None: any exported or
    /// address-taken function may be called back from any external call.
    pub callbacks: Option<HashSet<String>>,
    /// Bytes of hardware stack the program may use (Loom reserves 7,632);
    /// recursion is reported against it.
    pub stack_budget: u32,
}

impl Default for Options {
    fn default() -> Options {
        Options { rom_base: 0x80, tag: "unit".into(), callbacks: None, stack_budget: 7632 }
    }
}

pub struct Output {
    pub asm: String,
    pub errors: Vec<String>,
    pub warnings: Vec<String>,
    /// Functions whose code is larger than a ROM bank (name, bytes): they
    /// cannot link. The driver retries without inlining into them.
    pub oversized: Vec<(String, u32)>,
    /// Bytes of compiled stack (static frames) this module reserves.
    pub cstack_bytes: u32,
}

/// A LoROM bank: the most code one function (one SUPERFREE section) can hold.
pub const BANK_BYTES: u32 = 0x8000;

fn code_bytes(lines: &[Line]) -> u32 {
    lines
        .iter()
        .map(|l| match l {
            Line::Inst { mnem, mode, wide_imm } => asm::size(mnem, mode, *wide_imm),
            Line::Data(_, n) => *n,
            _ => 0,
        })
        .sum()
}

fn is_transfer(l: &Line) -> bool {
    matches!(l, Line::Inst { mnem: "bra" | "brl" | "jml" | "jmp" | "rtl" | "rts" | "rti", .. })
}

/// Splits a function's code (before branch relaxation) into pieces that
/// each fit a ROM bank. Cuts fall on labels; a piece that would fall
/// through into the next ends with `jml` to it; a branch to a label in
/// another piece becomes `jml` (a conditional one skips over it on the
/// inverted condition); a jump-table entry for a label in another piece goes
/// through a `jml` trampoline in the table's piece (`jmp (abs,x)` stays in
/// the program bank).
fn split_function(lines: Vec<Line>, fresh: &mut u32) -> Result<Vec<Vec<Line>>, String> {
    // Leaves room for the jml conversions and branch relaxation.
    const TARGET: u32 = 0x5800;
    let mut chunks: Vec<Vec<Line>> = vec![Vec::new()];
    let mut size = 0u32;
    let mut prev_indirect = false;
    for l in lines {
        let cut_here = matches!(l, Line::Label(_)) && size >= TARGET && !prev_indirect;
        if cut_here {
            chunks.push(Vec::new());
            size = 0;
        }
        prev_indirect = matches!(l, Line::Inst { mode: Mode::AbsIndX(_), .. }) || (prev_indirect && matches!(l, Line::Label(_)));
        size += code_bytes(std::slice::from_ref(&l));
        chunks.last_mut().unwrap().push(l);
    }
    // Which piece defines each label.
    let mut home: HashMap<String, usize> = HashMap::new();
    for (ci, c) in chunks.iter().enumerate() {
        for l in c {
            if let Line::Label(n) = l {
                home.insert(n.clone(), ci);
            }
        }
    }
    let n = chunks.len();
    // Every piece after the first starts with the label it was cut at.
    let firsts: Vec<Option<String>> = chunks
        .iter()
        .map(|c| match c.first() {
            Some(Line::Label(l)) => Some(l.clone()),
            _ => None,
        })
        .collect();
    // WLA-DX labels starting with `_` are local to their section: a label
    // reached from another piece gets a global alias at its definition.
    let alias = |l: &str| format!("lcx{}", l.trim_start_matches('_'));
    let mut crossed: HashSet<String> = HashSet::new();
    for (ci, c) in chunks.iter().enumerate() {
        for l in c {
            match l {
                Line::Inst { mode: Mode::Label(t), .. } if home.get(t).map_or(false, |&h| h != ci) => {
                    crossed.insert(t.clone());
                }
                Line::Data(text, _) if text.trim_start().starts_with(".dw") => {
                    for e in text.trim_start()[3..].split(',') {
                        let e = e.trim();
                        if home.get(e).map_or(false, |&h| h != ci) {
                            crossed.insert(e.to_string());
                        }
                    }
                }
                _ => {}
            }
        }
        if ci > 0 {
            if let Some(f) = &firsts[ci] {
                crossed.insert(f.clone());
            }
        }
    }
    let target = |l: &str| if l.starts_with('_') { alias(l) } else { l.to_string() };
    let mut out: Vec<Vec<Line>> = Vec::with_capacity(n);
    for (ci, c) in chunks.into_iter().enumerate() {
        let mut o: Vec<Line> = Vec::with_capacity(c.len() + 8);
        let mut trampolines: Vec<Line> = Vec::new();
        for l in c {
            match &l {
                Line::Inst { mnem, mode: Mode::Label(t), .. } if home.get(t).map_or(false, |&h| h != ci) => {
                    if asm::is_cond_branch(mnem) {
                        *fresh += 1;
                        let skip = format!("__ls{}", fresh);
                        o.push(Line::inst(asm::invert_branch(mnem), Mode::Label(skip.clone())));
                        o.push(Line::inst("jml", Mode::Long(asm::Expr::Sym(target(t), 0))));
                        o.push(Line::Label(skip));
                    } else if matches!(*mnem, "bra" | "brl" | "jmp") {
                        o.push(Line::inst("jml", Mode::Long(asm::Expr::Sym(target(t), 0))));
                    } else {
                        o.push(l.clone());
                    }
                }
                Line::Data(text, bytes) if text.trim_start().starts_with(".dw") => {
                    let entries: Vec<String> = text.trim_start()[3..].split(',').map(|e| e.trim().to_string()).collect();
                    let fixed: Vec<String> = entries
                        .into_iter()
                        .map(|e| {
                            if home.get(&e).map_or(false, |&h| h != ci) {
                                *fresh += 1;
                                let tr = format!("__lt{}", fresh);
                                trampolines.push(Line::Label(tr.clone()));
                                trampolines.push(Line::inst("jml", Mode::Long(asm::Expr::Sym(target(&e), 0))));
                                tr
                            } else {
                                e
                            }
                        })
                        .collect();
                    o.push(Line::Data(format!("  .dw {}", fixed.join(", ")), *bytes));
                }
                Line::Label(name) if crossed.contains(name) && name.starts_with('_') => {
                    let a = alias(name);
                    o.push(l);
                    o.push(Line::Label(a));
                }
                _ => o.push(l),
            }
        }
        // Falling through into the next piece.
        if ci + 1 < n && !o.iter().rev().find(|l| matches!(l, Line::Inst { .. } | Line::Data(..))).map_or(false, is_transfer) {
            match firsts[ci + 1].clone() {
                Some(lbl) => o.push(Line::inst("jml", Mode::Long(asm::Expr::Sym(target(&lbl), 0)))),
                None => return Err("internal: a split piece does not start with a label".into()),
            }
        }
        o.extend(trampolines);
        let mut f2 = *fresh;
        relax_branches(&mut o, &mut f2);
        *fresh = f2;
        let b = code_bytes(&o);
        if b > BANK_BYTES {
            return Err(format!(
                "a single stretch of this function compiles to {} bytes, more than a {}-byte ROM bank: split the function",
                b, BANK_BYTES
            ));
        }
        out.push(o);
    }
    Ok(out)
}


/// Library functions that never call back into the program: calls to them
/// need no protection against re-entry.
pub fn may_call_back(name: &str) -> bool {
    !matches!(
        name,
        "printf" | "sprintf" | "snprintf" | "fprintf" | "puts" | "putchar" | "memcpy" | "memmove" | "memset" | "memcmp"
            | "strlen" | "strcpy" | "strncpy" | "strcat" | "strcmp" | "strncmp" | "strchr" | "abs" | "atoi" | "malloc"
            | "free" | "calloc" | "exit" | "abort" | "rand" | "srand"
    )
}

pub(crate) fn interp_add(base: u32, delta: i64) -> u32 {
    (base & 0xff_0000) | (((base & 0xffff) as i64 + delta) as u32 & 0xffff)
}

/// A function's C name: private symbols carry a `lcs<tag>_<unit>_` prefix.
pub fn c_name(sym: &str) -> &str {
    if let Some(rest) = sym.strip_prefix("lcs") {
        if let Some(i) = rest.find('_') {
            let after = &rest[i + 1..];
            let digits = after.chars().take_while(|c| c.is_ascii_digit()).count();
            if digits > 0 && after[digits..].starts_with('_') {
                return &after[digits + 1..];
            }
        }
    }
    sym
}

fn sanitize(s: &str) -> String {
    s.chars().map(|c| if c.is_ascii_alphanumeric() || c == '_' { c } else { '_' }).collect()
}

/// Functions reachable from an interrupt root get their own copies
/// (`name__nmi`) for the interrupt context: static frames are not
/// reentrant, so a function the NMI shares with main-line code would have its
/// frame overwritten when the NMI arrives while main-line code is inside it.
pub fn clone_for_interrupts(m: &mut Module) {
    let index: HashMap<String, usize> = m.funcs.iter().enumerate().map(|(i, f)| (f.name.clone(), i)).collect();
    let roots: Vec<usize> = m.funcs.iter().enumerate().filter(|(_, f)| f.interrupt).map(|(i, _)| i).collect();
    if roots.is_empty() {
        return;
    }
    let mut reach: BTreeSet<usize> = BTreeSet::new();
    let mut stack = roots.clone();
    while let Some(v) = stack.pop() {
        for b in &m.funcs[v].blocks {
            for i in &b.insts {
                if let Inst::Call { callee: Callee::Direct(n), .. } = i {
                    if let Some(&j) = index.get(n) {
                        if !roots.contains(&j) && reach.insert(j) {
                            stack.push(j);
                        }
                    }
                }
            }
        }
    }
    let cloned: HashSet<String> = reach.iter().map(|&i| m.funcs[i].name.clone()).collect();
    let mut clones = Vec::new();
    for &i in &reach {
        let mut g = m.funcs[i].clone();
        g.name = format!("{}__nmi", g.name);
        g.exported = false;
        g.address_taken = false;
        g.interrupt = true;
        clones.push(g);
    }
    m.funcs.extend(clones);
    for f in m.funcs.iter_mut().filter(|f| f.interrupt) {
        for b in &mut f.blocks {
            for i in &mut b.insts {
                if let Inst::Call { callee: Callee::Direct(n), .. } = i {
                    if cloned.contains(n.as_str()) {
                        *n = format!("{}__nmi", n);
                    }
                }
            }
        }
    }
}

pub fn compile_module(m: &Module, opts: &Options) -> Output {
    let mut owned = m.clone();
    clone_for_interrupts(&mut owned);
    let tag = sanitize(&opts.tag);
    let mut errors = Vec::new();
    let mut warnings = Vec::new();
    let mut oversized: Vec<(String, u32)> = Vec::new();

    // Recursion: components of the call graph (with callbacks through
    // external code), the functions interrupts reach, and the candidates
    // for a frame on the hardware stack.
    let callbacks: HashSet<String> = match &opts.callbacks {
        Some(c) => c.clone(),
        None => owned.funcs.iter().filter(|f| f.exported || f.address_taken).map(|f| f.name.clone()).collect(),
    };
    let sccs = recursive_components(&owned, &callbacks);
    let mut scc_index: Vec<Option<usize>> = vec![None; owned.funcs.len()];
    for (k, comp) in sccs.iter().enumerate() {
        for &v in comp {
            scc_index[v] = Some(k);
        }
    }
    let interrupt_funcs = interrupt_reach(&owned);
    // Recursion within the C program itself (direct calls and calls through
    // pointers) gets frames on the hardware stack. A cycle only through
    // callbacks from external code keeps static frames, saved around the
    // external calls.
    let real: HashSet<usize> = recursive_components(&owned, &HashSet::new()).into_iter().flatten().collect();
    let dyn_cand: Vec<bool> = owned
        .funcs
        .iter()
        .enumerate()
        .map(|(v, f)| real.contains(&v) && !interrupt_funcs.contains(&f.name) && !f.variadic)
        .collect();
    for (v, f) in owned.funcs.iter_mut().enumerate() {
        if dyn_cand[v] {
            dynframe::materialize_slot_addresses(f);
        }
    }
    let m = &owned;

    // Which globals are near (bank $7E): our own .bss definitions.
    let mut near_globals = HashSet::new();
    for g in &m.globals {
        if g.init.is_some() && g.section == Section::Bss {
            near_globals.insert(g.name.clone());
        }
    }

    // Allocation per function.
    // Allocate callees before callers so a call clobbers only its callee
    // tree's direct-page words.
    let index: HashMap<&str, usize> = m.funcs.iter().enumerate().map(|(i, f)| (f.name.as_str(), i)).collect();
    let callees: Vec<Vec<Option<usize>>> = m
        .funcs
        .iter()
        .map(|f| {
            let mut v = Vec::new();
            for b in &f.blocks {
                for i in &b.insts {
                    if let Inst::Call { callee, .. } = i {
                        v.push(match callee {
                            Callee::Direct(n) => index.get(n.as_str()).copied(),
                            Callee::Indirect(_) => None,
                        });
                    }
                }
            }
            v
        })
        .collect();
    let mut post = Vec::new();
    let mut state = vec![0u8; m.funcs.len()];
    fn visit(v: usize, callees: &[Vec<Option<usize>>], state: &mut [u8], post: &mut Vec<usize>) {
        state[v] = 1;
        for c in callees[v].iter().flatten() {
            if state[*c] == 0 {
                visit(*c, callees, state, post);
            }
        }
        state[v] = 2;
        post.push(v);
    }
    for v in 0..m.funcs.len() {
        if state[v] == 0 {
            visit(v, &callees, &mut state, &mut post);
        }
    }
    let all_dp: Vec<u8> = DP_POOL.to_vec();
    let mut trans: Vec<Option<Vec<u8>>> = vec![None; m.funcs.len()];
    let mut allocs_opt: Vec<Option<Alloc>> = (0..m.funcs.len()).map(|_| None).collect();
    let mut is_dyn = vec![false; m.funcs.len()];
    let mut slot_x = vec![false; m.funcs.len()];
    // The absolute direct-page words a call into a dynamic-frame function
    // may change: those of the static-frame code its component calls (its
    // own words are in its frame).
    let scc_trans = |k: usize, trans: &[Option<Vec<u8>>], is_dyn: &[bool]| -> Vec<u8> {
        let mut t: Vec<u8> = Vec::new();
        for &u in &sccs[k] {
            if !is_dyn[u] {
                return all_dp.clone();
            }
            for c in &callees[u] {
                let words = match c {
                    Some(j) if scc_index[*j] == Some(k) => continue,
                    Some(j) => trans[*j].clone().unwrap_or_else(|| all_dp.clone()),
                    None => all_dp.clone(),
                };
                for w in words {
                    if !t.contains(&w) {
                        t.push(w);
                    }
                }
            }
        }
        t
    };
    for &v in &post {
        if dyn_cand[v] {
            // Its direct-page words are its own (relative to its frame), so
            // no call clobbers them.
            let none = |_: &Callee| Vec::new();
            let mut a = alloc::allocate_ext(&m.funcs[v], DP_POOL, &none, false);
            if dynframe::needs_slot_x(&a) {
                a = alloc::allocate_ext(&m.funcs[v], DP_POOL, &none, true);
                slot_x[v] = true;
            }
            if dynframe::homes_near(&a, &m.funcs[v]) {
                a.clean8 = alloc::clean8_ext(&m.funcs[v], true);
                is_dyn[v] = true;
                allocs_opt[v] = Some(a);
                continue;
            }
            slot_x[v] = false;
        }
        let clobber = |c: &Callee| -> Vec<u8> {
            match c {
                Callee::Direct(n) => match index.get(n.as_str()) {
                    Some(&j) if is_dyn[j] => scc_trans(scc_index[j].unwrap(), &trans, &is_dyn),
                    Some(&j) => trans[j].clone().unwrap_or_else(|| all_dp.clone()),
                    None => all_dp.clone(),
                },
                Callee::Indirect(_) => all_dp.clone(),
            }
        };
        let a = allocate(&m.funcs[v], DP_POOL, &clobber);
        let mut t = a.dp_used.clone();
        for c in &callees[v] {
            let words = match c {
                Some(j) if is_dyn[*j] => scc_trans(scc_index[*j].unwrap(), &trans, &is_dyn),
                Some(j) => trans[*j].clone().unwrap_or_else(|| all_dp.clone()),
                None => all_dp.clone(),
            };
            for w in words {
                if !t.contains(&w) {
                    t.push(w);
                }
            }
        }
        trans[v] = Some(t);
        allocs_opt[v] = Some(a);
    }
    let allocs: Vec<Alloc> = allocs_opt.into_iter().map(|a| a.unwrap()).collect();
    if std::env::var_os("LOOMCC_DEBUG_ALLOC").is_some() {
        for (f, a) in m.funcs.iter().zip(&allocs) {
            eprintln!("{}", loomcc_ir::print_func(f));
            for (i, h) in a.homes.iter().enumerate() {
                if *h != alloc::Home::None || a.forwarded[i] {
                    eprintln!("  %{} {:?}{}", i, h, if a.forwarded[i] { " fwd" } else { "" });
                }
            }
        }
    }

    let mut funcs = HashMap::new();
    for (f, a) in m.funcs.iter().zip(&allocs) {
        funcs.insert(
            f.name.clone(),
            FuncInfo {
                frame_sym: format!("lcf_{}", f.name),
                body_label: format!("lcb_{}", f.name),
                params: f.params.clone(),
                param_offsets: a.param_offsets.clone(),
                sret_offset: a.sret_offset,
                ret: f.ret,
                has_abi_entry: f.exported || f.address_taken || f.name == "main",
                param_homes: f.param_regs.iter().map(|r| r.map(|r| a.homes[r.0 as usize])).collect(),
                sret_home: f.sret_reg.map(|r| a.homes[r.0 as usize]),
                dyn_frame: None,
                dyn_slot_x: false,
                dyn_base: 0,
            },
        );
    }
    for (v, f) in m.funcs.iter().enumerate() {
        if is_dyn[v] {
            let fi = funcs.get_mut(&f.name).unwrap();
            fi.dyn_frame = Some(dynframe::frame_base(&allocs[v]) + allocs[v].frame_size);
            fi.dyn_base = dynframe::frame_base(&allocs[v]);
            fi.dyn_slot_x = slot_x[v];
        }
    }

    // The compiled stack: frames placed by call-graph depth (dynamic
    // frames live on the hardware stack instead).
    let static_sizes: Vec<u32> = (0..m.funcs.len()).map(|v| if is_dyn[v] { 0 } else { allocs[v].frame_size }).collect();
    let (frame_base, cstack_bytes, fe) = place_frames(m, &static_sizes, &callbacks);
    errors.extend(fe);

    let externs = m.extern_funcs.iter().map(|(n, p, _, _)| (n.clone(), p.clone())).collect();
    let mut scc_of = HashMap::new();
    let mut scc_members = Vec::new();
    for comp in &sccs {
        let id = scc_members.len();
        let mut members = Vec::new();
        for &v in comp {
            scc_of.insert(m.funcs[v].name.clone(), id);
            members.push((m.funcs[v].name.clone(), allocs[v].frame_size));
        }
        scc_members.push(members);
        // Stack use per level of recursion.
        let per_call: Vec<(String, u32)> = comp
            .iter()
            .filter(|&&v| is_dyn[v] && real.contains(&v))
            .map(|&v| {
                let f = &m.funcs[v];
                (f.name.clone(), dynframe::frame_base(&allocs[v]) + allocs[v].frame_size + 5 + dynframe::arg_bytes(&f.params, f.sret.is_some()))
            })
            .collect();
        if !per_call.is_empty() {
            let names: Vec<String> = comp.iter().filter(|v| real.contains(v)).map(|&v| format!("'{}'", c_name(&m.funcs[v].name))).collect();
            let worst = per_call.iter().map(|p| p.1).max().unwrap();
            let levels = opts.stack_budget / worst.max(1);
            warnings.push(format!(
                "recursion through {}: each call takes up to {} bytes of hardware stack and the depth is not bounded at compile time ({} levels fit in the {}-byte stack budget)",
                names.join(", "),
                worst,
                levels,
                opts.stack_budget
            ));
        }
    }
    let mi = ModuleInfo { funcs, near_globals, tag: tag.clone(), frame_base: frame_base.clone(), externs, interrupt_funcs, scc_of, scc_members };

    let mut out = String::new();
    let _ = writeln!(out, "; generated by loomcc");
    out.push_str(".include \"hdr.asm\"\n.accu 16\n.index 16\n.16bit\n");
    let _ = writeln!(out, ".BASE ${:02X}", opts.rom_base);
    let cstack = format!("lcc_cstack_{}", tag);
    let mut helpers: BTreeSet<Helper> = BTreeSet::new();
    let mut fresh = 0u32;
    for (fi, (f, a)) in m.funcs.iter().zip(&allocs).enumerate() {
        let info = &mi.funcs[&f.name];
        let mut g = Gen::new(f, a, &mi, format!("{}f{}", tag, fi));
        g.gen_function(info.has_abi_entry);
        errors.extend(g.errors.iter().map(|e| format!("{}: {}", f.name, e)));
        if info.dyn_frame.is_some() {
            let fs = mi.frame_symbol(&f.name);
            let mut text = String::new();
            for l in &g.lines {
                print_line(l, &mut text);
            }
            if text.contains(&fs) {
                errors.push(format!("{}: internal: a static frame reference in a function with a stack frame", f.name));
            }
        }
        helpers.extend(g.helpers.iter().copied());
        let mut lines = g.lines;
        peephole(&mut lines);
        let unsplit = lines.clone();
        relax_branches(&mut lines, &mut fresh);
        let bytes = code_bytes(&lines);
        // A function larger than a ROM bank goes into several sections
        // (F28); the driver first retries without inlining into it.
        let chunks = if bytes > BANK_BYTES {
            oversized.push((f.name.clone(), bytes));
            match split_function(unsplit, &mut fresh) {
                Ok(c) => c,
                Err(e) => {
                    errors.push(format!("{}: {}", f.name, e));
                    vec![lines]
                }
            }
        } else {
            vec![lines]
        };
        for (ci, chunk) in chunks.iter().enumerate() {
            if ci == 0 {
                let _ = writeln!(out, "\n.SECTION \"lcc.{}\" SUPERFREE", f.name);
            } else {
                let _ = writeln!(out, "\n.SECTION \"lcc.{}.part{}\" SUPERFREE", f.name, ci);
            }
            for l in chunk {
                print_line(&resolve_frames(l, &frame_base, &cstack), &mut out);
            }
            out.push_str(".ENDS\n");
        }
    }
    for h in &helpers {
        out.push_str(&helper_text(*h, &mi.helper_name(*h), &mi));
    }

    // Data.
    let bss: Vec<&Global> = m.globals.iter().filter(|g| g.init.is_some() && g.section == Section::Bss).collect();
    let data: Vec<&Global> = m.globals.iter().filter(|g| g.init.is_some() && g.section == Section::Data).collect();
    let rodata: Vec<&Global> = m.globals.iter().filter(|g| g.init.is_some() && g.section == Section::Rodata).collect();
    if !rodata.is_empty() {
        let _ = writeln!(out, "\n.SECTION \"lcc.rodata.{}\" SUPERFREE", tag);
        for g in &rodata {
            let _ = writeln!(out, "{}:", g.name);
            emit_bytes(&mut out, g);
        }
        out.push_str(".ENDS\n");
    }
    out.push_str("\n.BASE $00\n");
    if !data.is_empty() {
        out.push_str(".RAMSECTION \"ram{WLA_FILENAME}.data\" APPENDTO \"globram.data\"\n");
        for g in &data {
            let _ = writeln!(out, "{} dsb {}", g.name, g.size.max(1));
        }
        out.push_str(".ENDS\n");
        let _ = writeln!(out, ".BASE ${:02X}", opts.rom_base);
        out.push_str(".SECTION \"{WLA_FILENAME}.data\" APPENDTO \"glob.data\"\n");
        for g in &data {
            emit_bytes(&mut out, g);
        }
        out.push_str(".ENDS\n.BASE $00\n");
    }
    if !bss.is_empty() {
        out.push_str(".RAMSECTION \".bss\" BANK $7E SLOT 2\n");
        for g in &bss {
            let _ = writeln!(out, "{} dsb {}", g.name, g.size.max(1));
        }
        out.push_str(".ENDS\n");
    }
    for irq in [false, true] {
        if helpers.iter().any(|h| matches!(h, Helper::A32(_, i) if *i == irq)) {
            let _ = writeln!(out, ".RAMSECTION \"lcc.h32{}.{}\" BANK $7E SLOT 2", if irq { "i" } else { "" }, tag);
            let _ = writeln!(out, "lcc_h32{}_{} dsb 4", if irq { "i" } else { "" }, tag);
            out.push_str(".ENDS\n");
        }
    }
    if cstack_bytes > 0 {
        let _ = writeln!(out, ".RAMSECTION \"lcc.cstack.{}\" BANK $7E SLOT 2", tag);
        let _ = writeln!(out, "{} dsb {}", cstack, cstack_bytes);
        out.push_str(".ENDS\n");
    }
    let _ = writeln!(out, ".BASE ${:02X}", opts.rom_base);
    Output { asm: out, errors, warnings, cstack_bytes, oversized }
}

fn emit_bytes(out: &mut String, g: &Global) {
    let (bytes, relocs) = g.init.as_ref().unwrap();
    let mut rel: BTreeMap<u32, &DataReloc> = BTreeMap::new();
    for r in relocs {
        rel.insert(r.offset, r);
    }
    let mut i = 0usize;
    let mut run: Vec<String> = Vec::new();
    let flush = |run: &mut Vec<String>, out: &mut String| {
        for chunk in run.chunks(16) {
            let _ = writeln!(out, "  .db {}", chunk.join(","));
        }
        run.clear();
    };
    while i < bytes.len() {
        if let Some(r) = rel.get(&(i as u32)) {
            flush(&mut run, out);
            let target = if r.addend == 0 { r.target.clone() } else { format!("{}{:+}", r.target, r.addend) };
            if r.width >= 4 {
                let _ = writeln!(out, "  .dl {}", target);
                let _ = writeln!(out, "  .db $00");
            } else {
                let _ = writeln!(out, "  .dw {}", target);
            }
            i += r.width as usize;
            continue;
        }
        run.push(format!("${:02x}", bytes[i]));
        i += 1;
    }
    flush(&mut run, out);
    if bytes.is_empty() {
        let _ = writeln!(out, "  .db $00");
    }
}

/// Replaces `lcf_<f>+n` with the compiled-stack symbol plus the frame's
/// base.
fn resolve_frames(l: &Line, bases: &HashMap<String, u32>, cstack: &str) -> Line {
    use asm::{Expr, Mode};
    let fix = |e: &Expr| -> Expr {
        match e {
            Expr::Sym(s, o) if s.starts_with("lcf_") => {
                let base = bases.get(&s[4..]).copied().unwrap_or(0);
                Expr::Sym(cstack.to_string(), base as i64 + o)
            }
            other => other.clone(),
        }
    };
    match l {
        Line::Inst { mnem, mode, wide_imm } => {
            let mode = match mode {
                Mode::Imm(e) => Mode::Imm(fix(e)),
                Mode::Abs(e) => Mode::Abs(fix(e)),
                Mode::AbsX(e) => Mode::AbsX(fix(e)),
                Mode::AbsY(e) => Mode::AbsY(fix(e)),
                Mode::Long(e) => Mode::Long(fix(e)),
                Mode::LongX(e) => Mode::LongX(fix(e)),
                other => other.clone(),
            };
            Line::Inst { mnem, mode, wide_imm: *wide_imm }
        }
        other => other.clone(),
    }
}

/// Frames of functions that can be active together must not overlap: a
/// function's frame starts after every caller's frame ends (longest path in
/// the call graph). Calls to code outside the module may come back into any
/// function with an ABI entry.
fn place_frames(m: &Module, sizes: &[u32], callbacks: &HashSet<String>) -> (HashMap<String, u32>, u32, Vec<String>) {
    let n = m.funcs.len();
    let index: HashMap<&str, usize> = m.funcs.iter().enumerate().map(|(i, f)| (f.name.as_str(), i)).collect();
    let mut succ: Vec<BTreeSet<usize>> = vec![BTreeSet::new(); n];
    let callbacks: Vec<usize> = m.funcs.iter().enumerate().filter(|(_, f)| callbacks.contains(&f.name)).map(|(i, _)| i).collect();
    let address_taken: Vec<usize> = m.funcs.iter().enumerate().filter(|(_, f)| f.address_taken).map(|(i, _)| i).collect();
    for (i, f) in m.funcs.iter().enumerate() {
        for b in &f.blocks {
            for inst in &b.insts {
                if let Inst::Call { callee, .. } = inst {
                    match callee {
                        Callee::Direct(name) => match index.get(name.as_str()) {
                            Some(&j) => {
                                succ[i].insert(j);
                            }
                            None if !may_call_back(name) => {}
                            None => {
                                // External code may call back into the
                                // functions named as callbacks.
                                for &j in &callbacks {
                                    succ[i].insert(j);
                                }
                            }
                        },
                        Callee::Indirect(_) => {
                            for &j in &address_taken {
                                succ[i].insert(j);
                            }
                        }
                    }
                }
            }
        }
    }
    // Recursion check (Tarjan-free: DFS colouring).
    let mut errors = Vec::new();
    let mut state = vec![0u8; n];
    fn dfs(v: usize, succ: &[BTreeSet<usize>], state: &mut [u8], cyc: &mut Vec<usize>) {
        state[v] = 1;
        for &w in &succ[v] {
            if state[w] == 1 {
                cyc.push(w);
            } else if state[w] == 0 {
                dfs(w, succ, state, cyc);
            }
        }
        state[v] = 2;
    }
    let mut cyc = Vec::new();
    for v in 0..n {
        if state[v] == 0 {
            dfs(v, &succ, &mut state, &mut cyc);
        }
    }
    let recursive: HashSet<usize> = cyc.into_iter().collect();
    for &r in &recursive {
        let _ = r; // recursive calls save and restore frames (isel)
    }
    // Longest-path placement in topological order (ignoring back edges).
    let mut base = vec![0u32; n];
    let mut order = Vec::new();
    let mut seen = vec![false; n];
    fn topo(v: usize, succ: &[BTreeSet<usize>], seen: &mut [bool], order: &mut Vec<usize>) {
        seen[v] = true;
        for &w in &succ[v] {
            if !seen[w] {
                topo(w, succ, seen, order);
            }
        }
        order.push(v);
    }
    for v in 0..n {
        if !seen[v] {
            topo(v, &succ, &mut seen, &mut order);
        }
    }
    order.reverse();
    let mut pos = vec![0usize; n];
    for (i, &v) in order.iter().enumerate() {
        pos[v] = i;
    }
    for &v in &order {
        let end = base[v] + sizes[v];
        for &w in &succ[v] {
            if pos[w] > pos[v] {
                base[w] = base[w].max(end);
            }
        }
    }
    // Interrupt roots and everything they reach get frames past all others.
    let main_top = (0..n).map(|v| base[v] + sizes[v]).max().unwrap_or(0);
    let irq: Vec<usize> = m.funcs.iter().enumerate().filter(|(_, f)| f.interrupt).map(|(i, _)| i).collect();
    if !irq.is_empty() {
        let mut reach = HashSet::new();
        let mut stack = irq.clone();
        while let Some(v) = stack.pop() {
            if reach.insert(v) {
                stack.extend(succ[v].iter().copied());
            }
        }
        for &v in &order {
            if reach.contains(&v) {
                base[v] += main_top;
            }
        }
    }
    let total = (0..n).map(|v| base[v] + sizes[v]).max().unwrap_or(0);
    let map = m.funcs.iter().enumerate().map(|(i, f)| (f.name.clone(), base[i])).collect();
    (map, total, errors)
}

/// Small clean-ups on the emitted lines.
fn peephole(lines: &mut Vec<Line>) {
    use asm::Mode;
    let mut changed = true;
    while changed {
        changed = false;
        let mut i = 0;
        while i + 1 < lines.len() {
            // rep #$20 ; sep #$20  →  (nothing)
            if let (Line::Inst { mnem: "rep", mode: Mode::Imm(a), .. }, Line::Inst { mnem: "sep", mode: Mode::Imm(b), .. }) = (&lines[i], &lines[i + 1]) {
                if a == b {
                    lines.drain(i..i + 2);
                    changed = true;
                    continue;
                }
            }
            // bra L ; L:  →  L:
            if let (Line::Inst { mnem: "bra", mode: Mode::Label(t), .. }, Line::Label(l)) = (&lines[i], &lines[i + 1]) {
                if t == l {
                    lines.remove(i);
                    changed = true;
                    continue;
                }
            }
            // sta X ; lda X → sta X
            if let (Line::Inst { mnem: "sta", mode: m1, .. }, Line::Inst { mnem: "lda", mode: m2, .. }) = (&lines[i], &lines[i + 1]) {
                if m1 == m2 && matches!(m1, Mode::Dp(_) | Mode::Abs(_)) {
                    lines.remove(i + 1);
                    changed = true;
                    continue;
                }
            }
            i += 1;
        }
    }
}

fn helper_text(h: Helper, name: &str, mi: &ModuleInfo) -> String {
    let divu = mi.helper_name(Helper::DivU16);
    let divus = mi.helper_name(Helper::DivU16Soft);
    let body = match h {
        // A * X -> A (low 16 bits) on the CPU multiplier:
        // a_lo*b_lo + ((a_lo*b_hi + a_hi*b_lo) << 8), skipping zero high
        // bytes. $4216 is valid eight cycles after the write to $4203.
        Helper::Mul16 => format!(
            "{name}:\n  sta.b $18\n  stx.b $1a\n  sep #$20\n  sta.l $004202\n  txa\n  sta.l $004203\n  lda.b $19\n  ora.b $1b\n  bne {name}_wide\n  rep #$20\n  lda.l $004216\n  rtl\n{name}_wide:\n  rep #$20\n  lda.l $004216\n  sta.b $1c\n  sep #$20\n  lda.b $1b\n  beq {name}_a\n  sta.l $004203\n  nop\n  nop\n  nop\n  nop\n  lda.l $004216\n  clc\n  adc.b $1d\n  sta.b $1d\n{name}_a:\n  lda.b $19\n  beq {name}_b\n  sta.l $004202\n  lda.b $1a\n  sta.l $004203\n  nop\n  nop\n  nop\n  nop\n  lda.l $004216\n  clc\n  adc.b $1d\n  sta.b $1d\n{name}_b:\n  rep #$20\n  lda.b $1c\n  rtl\n"
        ),
        Helper::Mul16Soft => format!(
            "{name}:\n  sta.b $18\n  stx.b $1a\n  lda.w #$0000\n  ldx.b $1a\n  beq {name}_done\n{name}_loop:\n  lsr.b $1a\n  bcc {name}_skip\n  clc\n  adc.b $18\n{name}_skip:\n  asl.b $18\n  ldx.b $1a\n  bne {name}_loop\n{name}_done:\n  rtl\n"
        ),
        // A / X -> A quotient, X remainder (unsigned). Divisors under 256
        // use the CPU divider (results sixteen cycles after writing $4206).
        Helper::DivU16 => format!(
            "{name}:\n  cpx.w #$0100\n  bcs {name}_soft\n  sta.l $004204\n  txa\n  sep #$20\n  sta.l $004206\n  rep #$20\n  nop\n  nop\n  nop\n  nop\n  nop\n  nop\n  lda.l $004216\n  tax\n  lda.l $004214\n  rtl\n{name}_soft:\n  sta.b $18\n  stx.b $1a\n  lda.w #$0000\n  ldx.w #$0010\n{name}_loop:\n  asl.b $18\n  rol a\n  cmp.b $1a\n  bcc {name}_skip\n  sbc.b $1a\n  inc.b $18\n{name}_skip:\n  dex\n  bne {name}_loop\n  tax\n  lda.b $18\n  rtl\n"
        ),
        Helper::DivU16Soft => format!(
            "{name}:\n  sta.b $18\n  stx.b $1a\n  lda.w #$0000\n  ldx.w #$0010\n{name}_loop:\n  asl.b $18\n  rol a\n  cmp.b $1a\n  bcc {name}_skip\n  sbc.b $1a\n  inc.b $18\n{name}_skip:\n  dex\n  bne {name}_loop\n  tax\n  lda.b $18\n  rtl\n"
        ),
        // Signed: divide magnitudes, fix signs (quotient negative when the
        // signs differ, remainder takes the dividend's sign).
        Helper::DivS16 | Helper::DivS16Soft => {
            let core = if h == Helper::DivS16 { &divu } else { &divus };
            format!(
                "{name}:\n  stx.b $1e\n  pha\n  eor.b $1e\n  sta.b $1e\n  pla\n  pha\n  bpl {name}_a\n  eor.w #$ffff\n  inc a\n{name}_a:\n  pha\n  txa\n  bpl {name}_b\n  eor.w #$ffff\n  inc a\n{name}_b:\n  tax\n  pla\n  jsl {core}\n  sta.b $18\n  pla\n  bpl {name}_rpos\n  txa\n  eor.w #$ffff\n  inc a\n  tax\n{name}_rpos:\n  lda.b $1e\n  bpl {name}_qpos\n  lda.b $18\n  eor.w #$ffff\n  inc a\n  rtl\n{name}_qpos:\n  lda.b $18\n  rtl\n"
            )
        }
        Helper::JslR10 => format!("{name}:\n  jml [$1c]\n"),
        Helper::A32(code, irq) => {
            // Operands a at $18/$1a, b at $1c/$1e; result A (low), X (high).
            // Work RAM in bank $7E (a private copy for interrupt context).
            let ram = format!("lcc_h32{}_{}", if irq { "i" } else { "" }, mi.tag);
            let core = mi.helper_name(Helper::A32(1, irq));
            match code {
                0 => format!(
                    "{name}:\n  stz.w {ram}\n  stz.w {ram}+2\n{name}_loop:\n  lda.b $1c\n  ora.b $1e\n  beq {name}_done\n  lsr.b $1e\n  ror.b $1c\n  bcc {name}_skip\n  clc\n  lda.w {ram}\n  adc.b $18\n  sta.w {ram}\n  lda.w {ram}+2\n  adc.b $1a\n  sta.w {ram}+2\n{name}_skip:\n  asl.b $18\n  rol.b $1a\n  bra {name}_loop\n{name}_done:\n  lda.w {ram}\n  ldx.w {ram}+2\n  rtl\n"
                ),
                // Unsigned divide: quotient in $18/$1a, remainder in ram.
                1 => format!(
                    "{name}:\n  stz.w {ram}\n  stz.w {ram}+2\n  ldy.w #$0020\n{name}_loop:\n  asl.b $18\n  rol.b $1a\n  rol.w {ram}\n  rol.w {ram}+2\n  lda.w {ram}\n  sec\n  sbc.b $1c\n  tax\n  lda.w {ram}+2\n  sbc.b $1e\n  bcc {name}_skip\n  sta.w {ram}+2\n  stx.w {ram}\n  inc.b $18\n{name}_skip:\n  dey\n  bne {name}_loop\n  lda.b $18\n  ldx.b $1a\n  rtl\n"
                ),
                3 => format!("{name}:\n  jsl {core}\n  lda.w {ram}\n  ldx.w {ram}+2\n  rtl\n"),
                // Signed: magnitudes, then signs (quotient: a^b; remainder: a).
                _ => {
                    let rem = code == 4;
                    format!(
                        "{name}:\n  lda.b $1a\n  pha\n  eor.b $1e\n  pha\n  lda.b $1a\n  bpl {name}_a\n  lda.w #$0000\n  sec\n  sbc.b $18\n  sta.b $18\n  lda.w #$0000\n  sbc.b $1a\n  sta.b $1a\n{name}_a:\n  lda.b $1e\n  bpl {name}_b\n  lda.w #$0000\n  sec\n  sbc.b $1c\n  sta.b $1c\n  lda.w #$0000\n  sbc.b $1e\n  sta.b $1e\n{name}_b:\n  jsl {core}\n{pick}{sel}  bpl {name}_pos\n  lda.w #$0000\n  sec\n  sbc.b $18\n  sta.b $18\n  lda.w #$0000\n  sbc.b $1a\n  sta.b $1a\n{name}_pos:\n  lda.b $18\n  ldx.b $1a\n  rtl\n",
                        pick = if rem { format!("  lda.w {ram}\n  sta.b $18\n  lda.w {ram}+2\n  sta.b $1a\n") } else { String::new() },
                        // quotient sign on top of the stack; remainder sign below it
                        // Stack: quotient sign on top, dividend sign below.
                        sel = if rem { "  pla\n  pla\n".to_string() } else { "  pla\n  ply\n  ora.w #$0000\n".to_string() },
                    )
                }
            }
        }
    };
    format!("\n.SECTION \"{}\" SUPERFREE\n{}.ENDS\n", name, body)
}
