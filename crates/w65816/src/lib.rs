//! loomcc-w65816: the 65816 backend. Emits WLA-DX assembly that links beside
//! 816-tcc objects: every externally visible function has an 816-tcc ABI
//! entry under its C name; internal calls pass arguments straight into the
//! callee's static frame and return in A (A:X for 32-bit values).

pub mod alloc;
pub mod analysis;
pub mod asm;
pub mod isel;

use alloc::{allocate, Alloc, DP_POOL};
use asm::{print_line, relax_branches, Line};
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
    /// inputs for `jsl`/`jsr.l` targets).
    pub callbacks: HashSet<String>,
}

impl Default for Options {
    fn default() -> Options {
        Options { rom_base: 0x80, tag: "unit".into(), callbacks: HashSet::new() }
    }
}

pub struct Output {
    pub asm: String,
    pub errors: Vec<String>,
    /// Bytes of compiled stack (static frames) this module reserves.
    pub cstack_bytes: u32,
}

pub(crate) fn interp_add(base: u32, delta: i64) -> u32 {
    (base & 0xff_0000) | (((base & 0xffff) as i64 + delta) as u32 & 0xffff)
}

fn sanitize(s: &str) -> String {
    s.chars().map(|c| if c.is_ascii_alphanumeric() || c == '_' { c } else { '_' }).collect()
}

pub fn compile_module(m: &Module, opts: &Options) -> Output {
    let tag = sanitize(&opts.tag);
    let mut errors = Vec::new();

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
    for &v in &post {
        let clobber = |c: &Callee| -> Vec<u8> {
            match c {
                Callee::Direct(n) => match index.get(n.as_str()) {
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
            },
        );
    }

    // The compiled stack: frames placed by call-graph depth.
    let (frame_base, cstack_bytes, fe) = place_frames(m, &allocs, &opts.callbacks);
    errors.extend(fe);

    let externs = m.extern_funcs.iter().map(|(n, p, _, _)| (n.clone(), p.clone())).collect();
    let mi = ModuleInfo { funcs, near_globals, tag: tag.clone(), frame_base: frame_base.clone(), externs };

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
        helpers.extend(g.helpers.iter().copied());
        let mut lines = g.lines;
        peephole(&mut lines);
        relax_branches(&mut lines, &mut fresh);
        let _ = writeln!(out, "\n.SECTION \"lcc.{}\" SUPERFREE", f.name);
        for l in &lines {
            print_line(&resolve_frames(l, &frame_base, &cstack), &mut out);
        }
        out.push_str(".ENDS\n");
    }
    for h in &helpers {
        out.push_str(&helper_text(*h, &mi.helper_name(*h)));
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
    if cstack_bytes > 0 {
        let _ = writeln!(out, ".RAMSECTION \"lcc.cstack.{}\" BANK $7E SLOT 2", tag);
        let _ = writeln!(out, "{} dsb {}", cstack, cstack_bytes);
        out.push_str(".ENDS\n");
    }
    let _ = writeln!(out, ".BASE ${:02X}", opts.rom_base);
    Output { asm: out, errors, cstack_bytes }
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
fn place_frames(m: &Module, allocs: &[Alloc], callbacks: &HashSet<String>) -> (HashMap<String, u32>, u32, Vec<String>) {
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
        errors.push(format!("{}: recursion is not supported yet (static frames)", m.funcs[r].name));
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
        let end = base[v] + allocs[v].frame_size;
        for &w in &succ[v] {
            if pos[w] > pos[v] {
                base[w] = base[w].max(end);
            }
        }
    }
    // Interrupt roots and everything they reach get frames past all others.
    let main_top = (0..n).map(|v| base[v] + allocs[v].frame_size).max().unwrap_or(0);
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
    let total = (0..n).map(|v| base[v] + allocs[v].frame_size).max().unwrap_or(0);
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

fn helper_text(h: Helper, name: &str) -> String {
    let body = match h {
        // A * X -> A (low 16 bits). Shift-add over the multiplier's bits.
        Helper::Mul16 => format!(
            "{name}:\n  sta.b $18\n  stx.b $1a\n  lda.w #$0000\n  ldx.b $1a\n  beq {name}_done\n{name}_loop:\n  lsr.b $1a\n  bcc {name}_skip\n  clc\n  adc.b $18\n{name}_skip:\n  asl.b $18\n  ldx.b $1a\n  bne {name}_loop\n{name}_done:\n  rtl\n"
        ),
        // A / X -> A quotient, X remainder (unsigned).
        Helper::DivU16 => format!(
            "{name}:\n  sta.b $18\n  stx.b $1a\n  lda.w #$0000\n  ldx.w #$0010\n{name}_loop:\n  asl.b $18\n  rol a\n  cmp.b $1a\n  bcc {name}_skip\n  sbc.b $1a\n  inc.b $18\n{name}_skip:\n  dex\n  bne {name}_loop\n  tax\n  lda.b $18\n  rtl\n"
        ),
        // Signed: divide magnitudes, fix signs (quotient negative when the
        // signs differ, remainder takes the dividend's sign).
        Helper::DivS16 => format!(
            "{name}:\n  stx.b $1a\n  sta.b $18\n  eor.b $1a\n  pha\n  lda.b $18\n  pha\n  bpl {name}_a\n  eor.w #$ffff\n  inc a\n  sta.b $18\n{name}_a:\n  lda.b $1a\n  bpl {name}_b\n  eor.w #$ffff\n  inc a\n  sta.b $1a\n{name}_b:\n  lda.w #$0000\n  ldx.w #$0010\n{name}_loop:\n  asl.b $18\n  rol a\n  cmp.b $1a\n  bcc {name}_skip\n  sbc.b $1a\n  inc.b $18\n{name}_skip:\n  dex\n  bne {name}_loop\n  tax\n  pla\n  bpl {name}_rpos\n  txa\n  eor.w #$ffff\n  inc a\n  tax\n{name}_rpos:\n  pla\n  bpl {name}_qpos\n  lda.b $18\n  eor.w #$ffff\n  inc a\n  rtl\n{name}_qpos:\n  lda.b $18\n  rtl\n"
        ),
        Helper::JslR10 => format!("{name}:\n  jml [$1c]\n"),
    };
    format!("\n.SECTION \"{}\" SUPERFREE\n{}.ENDS\n", name, body)
}
