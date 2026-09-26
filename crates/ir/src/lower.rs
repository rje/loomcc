//! Lowering from sema's typed tree to the IR.

use crate::ir::*;
use loomcc_pp::{Diag, Loc};
use loomcc_sema::hir::{self, ExprKind, GlobalId, Linkage, LocalId, StmtKind};
use loomcc_sema::types::{Ty, TyKind, Types};
use std::collections::HashMap;

/// Lowers checked units into one module (whole program). Statics are made
/// unique per unit; external names are shared.
pub fn lower_units(units: &[hir::Unit]) -> (Module, Vec<(usize, Diag)>) {
    lower_units_prefixed(units, "")
}

/// As `lower_units`, with `prefix` in every private symbol so separately
/// compiled objects never share a static's name.
pub fn lower_units_prefixed(units: &[hir::Unit], prefix: &str) -> (Module, Vec<(usize, Diag)>) {
    let mut m = Module::default();
    let mut diags = Vec::new();
    let clean: String = prefix.chars().map(|c| if c.is_ascii_alphanumeric() || c == '_' { c } else { '_' }).collect();
    PREFIX.with(|p| *p.borrow_mut() = clean);
    for (ui, u) in units.iter().enumerate() {
        let mut ds = Vec::new();
        lower_unit(u, ui, &mut m, &mut ds);
        diags.extend(ds.into_iter().map(|d| (ui, d)));
    }
    // Globals declared in one unit and defined in another: keep the
    // definition.
    let mut seen: HashMap<String, usize> = HashMap::new();
    let mut globals: Vec<Global> = Vec::new();
    for g in std::mem::take(&mut m.globals) {
        match seen.get(&g.name) {
            Some(&i) => {
                if globals[i].init.is_none() && g.init.is_some() {
                    globals[i] = g;
                } else if globals[i].init.is_some() && g.init.is_some() && g.exported {
                    // Two definitions: tentative definitions merge (keep the
                    // non-zero one).
                    let nonzero = |x: &Global| x.init.as_ref().map_or(false, |(b, r)| b.iter().any(|&v| v != 0) || !r.is_empty());
                    if nonzero(&g) && !nonzero(&globals[i]) {
                        globals[i] = g;
                    }
                }
            }
            None => {
                seen.insert(g.name.clone(), globals.len());
                globals.push(g);
            }
        }
    }
    m.globals = globals;
    let defined: std::collections::HashSet<String> = m.funcs.iter().map(|f| f.name.clone()).collect();
    let mut ext: Vec<(String, Vec<ParamKind>, Option<IrTy>, bool)> = Vec::new();
    for e in std::mem::take(&mut m.extern_funcs) {
        if !defined.contains(&e.0) && !ext.iter().any(|x| x.0 == e.0) {
            ext.push(e);
        }
    }
    m.extern_funcs = ext;
    (m, diags)
}

thread_local! {
    static PREFIX: std::cell::RefCell<String> = const { std::cell::RefCell::new(String::new()) };
}

/// The assembler symbol for a global.
pub fn symbol_name(unit: &hir::Unit, unit_index: usize, g: GlobalId) -> String {
    let gl = unit.global(g);
    if gl.linkage == Linkage::External {
        gl.name.clone()
    } else {
        let clean: String = gl.name.chars().map(|c| if c.is_ascii_alphanumeric() || c == '_' { c } else { '_' }).collect();
        let pre = PREFIX.with(|p| p.borrow().clone());
        format!("lcs{}{}_{}", pre, unit_index, clean.trim_start_matches('_'))
    }
}

pub fn ir_ty(types: &Types, t: Ty) -> Option<IrTy> {
    match types.kind(t) {
        TyKind::Bool => Some(IrTy::I8),
        TyKind::Int(_) | TyKind::Enum(_) => match types.size(t) {
            1 => Some(IrTy::I8),
            2 => Some(IrTy::I16),
            4 => Some(IrTy::I32),
            _ => None,
        },
        TyKind::Ptr(_) => Some(IrTy::Ptr),
        _ => None,
    }
}

fn lower_unit(u: &hir::Unit, ui: usize, m: &mut Module, diags: &mut Vec<Diag>) {
    let types = &u.types;
    for (i, g) in u.globals.iter().enumerate() {
        let gid = GlobalId(i as u32);
        let name = symbol_name(u, ui, gid);
        if g.is_func {
            if g.func.is_none() {
                continue;
            }
            continue;
        }
        let size = types.size(g.ty).max(if types.is_complete(g.ty) { 0 } else { 0 }) as u32;
        let section = match g.section {
            hir::Section::Bss => Section::Bss,
            hir::Section::Data => Section::Data,
            hir::Section::Rodata => Section::Rodata,
        };
        let init = if g.defined {
            g.init.as_ref().map(|si| {
                let relocs = si
                    .relocs
                    .iter()
                    .map(|r| DataReloc { offset: r.offset as u32, target: symbol_name(u, ui, r.target), addend: r.addend, width: r.width })
                    .collect();
                (si.bytes.clone(), relocs)
            })
        } else {
            None
        };
        m.globals.push(Global {
            name,
            size,
            align: types.align(g.ty) as u32,
            section,
            exported: g.linkage == Linkage::External,
            init,
        });
    }
    for (i, g) in u.globals.iter().enumerate() {
        let Some(func) = &g.func else { continue };
        let gid = GlobalId(i as u32);
        let mut l = Lowerer::new(u, ui, gid, func, m);
        l.lower_function();
        let (f, externs, ds) = l.finish();
        diags.extend(ds);
        for e in externs {
            if !m.extern_funcs.iter().any(|x| x.0 == e.0) {
                m.extern_funcs.push(e);
            }
        }
        m.funcs.push(f);
    }
}

#[derive(Clone, Debug)]
enum Home {
    Reg(VReg),
    Slot(SlotId),
    Global(String),
}

#[derive(Clone, Debug)]
enum LV {
    Reg(VReg),
    Mem(Addr, MemTy, bool),
    /// Bit-field: unit address, bit, width, unit type, signed, volatile.
    Bit(Addr, u32, u32, IrTy, bool, bool),
}

#[derive(Clone, Copy, Debug, PartialEq)]
enum MemTy {
    Scalar(IrTy),
    Agg(u32),
}

struct Lowerer<'a> {
    u: &'a hir::Unit,
    ui: usize,
    hf: &'a hir::Function,
    f: Func,
    cur: BlockId,
    homes: Vec<Option<Home>>,
    breaks: Vec<BlockId>,
    continues: Vec<BlockId>,
    cases: Vec<Vec<BlockId>>,
    labels: HashMap<String, BlockId>,
    externs: Vec<(String, Vec<ParamKind>, Option<IrTy>, bool)>,
    diags: Vec<Diag>,
    ret_ty: Ty,
}

impl<'a> Lowerer<'a> {
    fn new(u: &'a hir::Unit, ui: usize, gid: GlobalId, hf: &'a hir::Function, _m: &Module) -> Lowerer<'a> {
        let g = u.global(gid);
        let sig = u.types.sig(g.ty).unwrap().clone();
        let f = Func {
            name: symbol_name(u, ui, gid),
            exported: g.linkage == Linkage::External,
            params: Vec::new(),
            param_regs: Vec::new(),
            param_slots: Vec::new(),
            ret: ir_ty(&u.types, sig.ret),
            sret: if u.types.is_record(sig.ret) { Some(u.types.size(sig.ret) as u32) } else { None },
            sret_reg: None,
            vregs: Vec::new(),
            slots: Vec::new(),
            blocks: vec![Block { insts: vec![], term: Term::Unreachable }],
            interrupt: hf.interrupt,
            variadic: hf.variadic,
            address_taken: g.address_taken,
            signed: Vec::new(),
        };
        Lowerer {
            u,
            ui,
            hf,
            f,
            cur: BlockId(0),
            homes: vec![None; hf.locals.len()],
            breaks: vec![],
            continues: vec![],
            cases: vec![],
            labels: HashMap::new(),
            externs: vec![],
            diags: vec![],
            ret_ty: sig.ret,
        }
    }

    fn finish(self) -> (Func, Vec<(String, Vec<ParamKind>, Option<IrTy>, bool)>, Vec<Diag>) {
        (self.f, self.externs, self.diags)
    }

    fn types(&self) -> &'a Types {
        &self.u.types
    }

    fn error(&mut self, loc: Loc, msg: impl Into<String>) {
        self.diags.push(Diag::error(loc, msg));
    }

    fn ity(&mut self, t: Ty, loc: Loc) -> IrTy {
        match ir_ty(self.types(), t) {
            Some(i) => i,
            None => {
                let d = self.types().display(t);
                self.error(loc, format!("type '{}' is not supported by the 65816 backend", d));
                IrTy::I16
            }
        }
    }

    fn new_block(&mut self) -> BlockId {
        self.f.blocks.push(Block { insts: vec![], term: Term::Unreachable });
        BlockId((self.f.blocks.len() - 1) as u32)
    }

    fn emit(&mut self, i: Inst) {
        self.f.blocks[self.cur.0 as usize].insts.push(i);
    }

    fn terminate(&mut self, t: Term) {
        let b = &mut self.f.blocks[self.cur.0 as usize];
        if matches!(b.term, Term::Unreachable) {
            b.term = t;
        }
    }

    /// Ends the current block with `t` and continues in a fresh block
    /// (unreachable unless something jumps there).
    fn seal(&mut self, t: Term) {
        self.terminate(t);
        self.cur = self.new_block();
    }

    fn switch_to(&mut self, b: BlockId) {
        self.terminate(Term::Jmp(b));
        self.cur = b;
    }

    fn vreg(&mut self, t: IrTy) -> VReg {
        self.f.new_vreg(t)
    }

    fn slot(&mut self, size: u32, align: u32, name: &str) -> SlotId {
        self.f.slots.push(Slot { size: size.max(1), align, name: name.to_string() });
        SlotId((self.f.slots.len() - 1) as u32)
    }

    fn sym(&self, g: GlobalId) -> String {
        symbol_name(self.u, self.ui, g)
    }

    // ------------------------------------------------------------------ functions

    fn lower_function(&mut self) {
        // Parameters.
        for (pi, &lid) in self.hf.params.iter().enumerate() {
            let l = &self.hf.locals[lid.0 as usize];
            let _ = pi;
            if let Some(it) = ir_ty(self.types(), l.ty) {
                let r = self.vreg(it);
                if self.types().is_signed(l.ty) {
                    self.f.signed[r.0 as usize] = true;
                }
                self.f.params.push(ParamKind::Scalar(it));
                self.f.param_regs.push(Some(r));
                self.f.param_slots.push(None);
                if l.address_taken {
                    let size = self.types().size(l.ty) as u32;
                    let align = self.types().align(l.ty) as u32;
                    let s = self.slot(size, align, &l.name);
                    self.emit(Inst::Store { addr: Addr { base: Base::Slot(s), offset: 0, index: None }, src: Operand::Reg(r), ty: it, volatile: false });
                    self.homes[lid.0 as usize] = Some(Home::Slot(s));
                } else {
                    self.homes[lid.0 as usize] = Some(Home::Reg(r));
                }
            } else if self.types().is_record(l.ty) {
                let size = self.types().size(l.ty) as u32;
                let align = self.types().align(l.ty) as u32;
                let s = self.slot(size, align, &l.name);
                self.f.params.push(ParamKind::Aggregate(size));
                self.f.param_regs.push(None);
                self.f.param_slots.push(Some(s));
                self.homes[lid.0 as usize] = Some(Home::Slot(s));
            } else {
                let d = self.types().display(l.ty);
                self.error(l.loc, format!("parameter type '{}' is not supported", d));
                let r = self.vreg(IrTy::I16);
                self.f.params.push(ParamKind::Scalar(IrTy::I16));
                self.f.param_regs.push(Some(r));
                self.f.param_slots.push(None);
                self.homes[lid.0 as usize] = Some(Home::Reg(r));
            }
        }
        if self.f.sret.is_some() {
            let r = self.vreg(IrTy::Ptr);
            self.f.sret_reg = Some(r);
        }
        let body = &self.hf.body;
        self.stmt(body);
        // Falling off the end: return (main returns 0).
        let ret = if self.f.ret.is_some() {
            if self.f.name == "main" { Some(Operand::Imm(0)) } else { Some(Operand::Imm(0)) }
        } else {
            None
        };
        self.terminate(Term::Ret(ret));
        // Unresolved gotos are sema errors; nothing to do here.
    }

    fn home_of(&mut self, lid: LocalId) -> Home {
        if let Some(h) = &self.homes[lid.0 as usize] {
            return h.clone();
        }
        let l = &self.hf.locals[lid.0 as usize];
        let h = if let Some(g) = l.static_global {
            Home::Global(self.sym(g))
        } else if let (Some(it), false) = (ir_ty(self.types(), l.ty), l.address_taken) {
            if self.types().is_volatile(l.ty) {
                // Volatile locals live in memory so every access happens.
                let s = self.slot(it.size(), it.size().min(2), &l.name.clone());
                Home::Slot(s)
            } else {
                let signed = self.types().is_signed(l.ty);
                let r = self.vreg(it);
                self.f.signed[r.0 as usize] = signed;
                Home::Reg(r)
            }
        } else {
            let size = self.types().size(l.ty) as u32;
            let align = self.types().align(l.ty) as u32;
            let name = l.name.clone();
            Home::Slot(self.slot(size, align, &name))
        };
        self.homes[lid.0 as usize] = Some(h.clone());
        h
    }

    // ------------------------------------------------------------------ statements

    fn stmt(&mut self, s: &hir::Stmt) {
        match &s.kind {
            StmtKind::Block(v) => {
                for st in v {
                    self.stmt(st);
                }
            }
            StmtKind::Expr(e) => {
                self.effect(e);
            }
            StmtKind::Decl(lid, init) => {
                let _ = self.home_of(*lid);
                for st in init {
                    self.stmt(st);
                }
            }
            StmtKind::If(c, a, b) => {
                let (bt, bf, end) = (self.new_block(), self.new_block(), self.new_block());
                self.cond_branch(c, bt, bf);
                self.cur = bt;
                self.stmt(a);
                self.terminate(Term::Jmp(end));
                self.cur = bf;
                if let Some(b) = b {
                    self.stmt(b);
                }
                self.terminate(Term::Jmp(end));
                self.cur = end;
            }
            StmtKind::While(c, body) => {
                let (head, bodyb, end) = (self.new_block(), self.new_block(), self.new_block());
                self.switch_to(head);
                self.cond_branch(c, bodyb, end);
                self.cur = bodyb;
                self.breaks.push(end);
                self.continues.push(head);
                self.stmt(body);
                self.breaks.pop();
                self.continues.pop();
                self.terminate(Term::Jmp(head));
                self.cur = end;
            }
            StmtKind::DoWhile(body, c) => {
                let (bodyb, cond, end) = (self.new_block(), self.new_block(), self.new_block());
                self.switch_to(bodyb);
                self.breaks.push(end);
                self.continues.push(cond);
                self.stmt(body);
                self.breaks.pop();
                self.continues.pop();
                self.switch_to(cond);
                self.cond_branch(c, bodyb, end);
                self.cur = end;
            }
            StmtKind::For(init, c, step, body) => {
                if let Some(i) = init {
                    self.stmt(i);
                }
                let (head, bodyb, stepb, end) = (self.new_block(), self.new_block(), self.new_block(), self.new_block());
                self.switch_to(head);
                match c {
                    Some(c) => self.cond_branch(c, bodyb, end),
                    None => self.terminate(Term::Jmp(bodyb)),
                }
                self.cur = bodyb;
                self.breaks.push(end);
                self.continues.push(stepb);
                self.stmt(body);
                self.breaks.pop();
                self.continues.pop();
                self.switch_to(stepb);
                if let Some(s) = step {
                    self.effect(s);
                }
                self.terminate(Term::Jmp(head));
                self.cur = end;
            }
            StmtKind::Switch(c, body, cases, default) => {
                let ty = self.ity(c.ty, c.loc);
                let v = self.value(c);
                let n = cases.len() + default.map_or(0, |_| 1);
                let blocks: Vec<BlockId> = (0..cases.len() as u32 + default.map_or(0, |_| 1) as u32).map(|_| self.new_block()).collect();
                let _ = n;
                // Case label index -> block: labels are numbered in the order
                // sema met them; map by the label index recorded in cases.
                let mut label_block: HashMap<u32, BlockId> = HashMap::new();
                let mut k = 0;
                for (_, _, idx) in cases {
                    label_block.insert(*idx, blocks[k]);
                    k += 1;
                }
                if let Some(d) = default {
                    label_block.insert(*d, blocks[k]);
                }
                let end = self.new_block();
                let default_block = default.map(|d| label_block[&d]).unwrap_or(end);
                let mut list = Vec::new();
                let mut ranges = Vec::new();
                for (lo, hi, idx) in cases {
                    let b = label_block[idx];
                    if hi - lo < 64 {
                        for v in *lo..=*hi {
                            list.push((v, b));
                        }
                    } else {
                        ranges.push((*lo, *hi, b));
                    }
                }
                // Large ranges become compare chains before the switch.
                for (lo, hi, b) in ranges {
                    let (ok_lo, next) = (self.new_block(), self.new_block());
                    let signed = self.types().is_signed(c.ty);
                    let (ge, le) = if signed { (Cond::GeS, Cond::LeS) } else { (Cond::GeU, Cond::LeU) };
                    self.terminate(Term::BrCmp { cc: ge, ty, a: v.clone(), b: Operand::Imm(lo), t: ok_lo, f: next });
                    self.cur = ok_lo;
                    self.terminate(Term::BrCmp { cc: le, ty, a: v.clone(), b: Operand::Imm(hi), t: b, f: next });
                    self.cur = next;
                }
                self.terminate(Term::Switch { val: v, ty, cases: list, default: default_block });
                let mut idx_blocks: Vec<BlockId> = Vec::new();
                // Keep a lookup from label index to block for CaseLabel.
                let max_idx = label_block.keys().max().copied().unwrap_or(0);
                idx_blocks.resize(max_idx as usize + 1, end);
                for (i, b) in &label_block {
                    idx_blocks[*i as usize] = *b;
                }
                self.cases.push(idx_blocks);
                self.breaks.push(end);
                self.cur = self.new_block(); // unreachable code before the first label
                self.stmt(body);
                self.breaks.pop();
                self.cases.pop();
                self.switch_to(end);
            }
            StmtKind::CaseLabel(idx) => {
                let b = self.cases.last().map(|c| c[*idx as usize]).unwrap();
                self.switch_to(b);
            }
            StmtKind::Label(l) => {
                let b = self.label_block(l);
                self.switch_to(b);
            }
            StmtKind::Goto(l) => {
                let b = self.label_block(l);
                self.seal(Term::Jmp(b));
            }
            StmtKind::Break => {
                let b = *self.breaks.last().unwrap();
                self.seal(Term::Jmp(b));
            }
            StmtKind::Continue => {
                let b = *self.continues.last().unwrap();
                self.seal(Term::Jmp(b));
            }
            StmtKind::Return(e) => {
                match e {
                    None => self.seal(Term::Ret(None)),
                    Some(e) => {
                        if self.f.sret.is_some() {
                            let src = self.agg_addr(e);
                            let size = self.f.sret.unwrap();
                            let dst = Addr { base: Base::Reg(self.f.sret_reg.unwrap()), offset: 0, index: None };
                            self.emit(Inst::Memcpy { dst, src, size });
                            self.seal(Term::Ret(None));
                        } else {
                            let v = self.value(e);
                            let _ = self.ret_ty;
                            self.seal(Term::Ret(Some(v)));
                        }
                    }
                }
            }
            StmtKind::Empty => {}
        }
    }

    fn label_block(&mut self, l: &str) -> BlockId {
        if let Some(b) = self.labels.get(l) {
            return *b;
        }
        let b = self.new_block();
        self.labels.insert(l.to_string(), b);
        b
    }

    // ------------------------------------------------------------------ expressions

    /// Evaluates for side effects only.
    fn effect(&mut self, e: &hir::Expr) {
        match &e.kind {
            ExprKind::Cast(inner) if self.types().is_void(e.ty) => self.effect(inner),
            ExprKind::Comma(a, b) => {
                self.effect(a);
                self.effect(b);
            }
            ExprKind::IncDec(lv, inc, _, step) => {
                // Post/pre does not matter when the value is unused.
                self.incdec(lv, *inc, true, *step, e.loc);
            }
            ExprKind::Assign(..) | ExprKind::CompoundAssign(..) | ExprKind::PtrCompoundAssign(..) => {
                self.value_or_void(e);
            }
            ExprKind::Call(..) | ExprKind::StmtExpr(..) | ExprKind::ZeroInit(_) | ExprKind::Cond(..) | ExprKind::LogAnd(..) | ExprKind::LogOr(..) => {
                self.value_or_void(e);
            }
            _ => {
                if e.is_lvalue() {
                    // Reading a volatile object is a side effect.
                    if self.types().is_volatile(e.ty) && ir_ty(self.types(), e.ty).is_some() {
                        self.value(e);
                    } else {
                        // Still evaluate subexpressions with effects.
                        self.effect_children(e);
                    }
                } else if !self.types().is_void(e.ty) && ir_ty(self.types(), e.ty).is_some() {
                    self.effect_children(e);
                } else {
                    self.value_or_void(e);
                }
            }
        }
    }

    fn effect_children(&mut self, e: &hir::Expr) {
        match &e.kind {
            ExprKind::Binary(_, a, b) | ExprKind::Cmp(_, a, b) | ExprKind::PtrAdd(a, b, _) | ExprKind::PtrDiff(a, b, _) => {
                self.effect(a);
                self.effect(b);
            }
            ExprKind::Unary(_, a) | ExprKind::Cast(a) | ExprKind::Member(a, _) | ExprKind::AddrOf(a) => self.effect(a),
            ExprKind::Deref(p) => self.effect(p),
            ExprKind::BitField(a, ..) => self.effect(a),
            ExprKind::CompoundLiteral(..) => {
                self.lvalue(e);
            }
            _ => {}
        }
    }

    fn value_or_void(&mut self, e: &hir::Expr) -> Option<Operand> {
        if self.types().is_void(e.ty) {
            match &e.kind {
                ExprKind::Call(..) => {
                    self.call(e, None);
                }
                ExprKind::Cast(inner) => self.effect(inner),
                ExprKind::Comma(a, b) => {
                    self.effect(a);
                    self.value_or_void(b);
                }
                ExprKind::Cond(c, a, b) => {
                    let (bt, bf, end) = (self.new_block(), self.new_block(), self.new_block());
                    self.cond_branch(c, bt, bf);
                    self.cur = bt;
                    self.value_or_void(a);
                    self.terminate(Term::Jmp(end));
                    self.cur = bf;
                    self.value_or_void(b);
                    self.terminate(Term::Jmp(end));
                    self.cur = end;
                }
                ExprKind::StmtExpr(stmts, last) => {
                    for s in stmts {
                        self.stmt(s);
                    }
                    if let Some(l) = last {
                        self.value_or_void(l);
                    }
                }
                ExprKind::ZeroInit(target) => {
                    let (addr, size) = match self.lvalue(target) {
                        LV::Mem(a, MemTy::Agg(n), _) => (Some(a), n),
                        LV::Mem(a, MemTy::Scalar(t), _) => (Some(a), t.size()),
                        LV::Reg(r) => {
                            self.emit(Inst::Mov { dst: r, src: Operand::Imm(0) });
                            (None, 0)
                        }
                        LV::Bit(..) => (None, 0),
                    };
                    if let Some(a) = addr {
                        self.emit(Inst::Memset { dst: a, val: 0, size });
                    }
                }
                ExprKind::VaStart(_) | ExprKind::VaEnd(_) => {
                    self.error(e.loc, "variadic functions are not supported by the 65816 backend");
                }
                ExprKind::IntConst(_) => {}
                _ => {
                    self.error(e.loc, "unsupported void expression");
                }
            }
            return None;
        }
        if self.types().is_record(e.ty) {
            // Aggregate-valued expression evaluated for effect.
            let _ = self.agg_addr(e);
            return None;
        }
        Some(self.value(e))
    }

    fn is_signed(&self, t: Ty) -> bool {
        self.types().is_signed(t)
    }

    /// The value of a scalar expression.
    fn value(&mut self, e: &hir::Expr) -> Operand {
        let loc = e.loc;
        if e.is_lvalue() {
            if self.types().is_record(e.ty) || self.types().is_array(e.ty) {
                self.error(loc, "aggregate used as a scalar value");
                return Operand::Imm(0);
            }
            let lv = self.lvalue(e);
            return self.load(&lv);
        }
        let ty = e.ty;
        match &e.kind {
            ExprKind::IntConst(v) => {
                let it = self.ity(ty, loc);
                Operand::Imm(it.zext(*v))
            }
            ExprKind::FloatConst(_) => {
                self.error(loc, "floating point is not supported by the 65816 backend");
                Operand::Imm(0)
            }
            ExprKind::Binary(op, a, b) => {
                let it = self.ity(ty, loc);
                let signed = self.is_signed(ty);
                let bop = match op {
                    hir::BinOp::Add => BinOp::Add,
                    hir::BinOp::Sub => BinOp::Sub,
                    hir::BinOp::Mul => BinOp::Mul,
                    hir::BinOp::Div => if signed { BinOp::DivS } else { BinOp::DivU },
                    hir::BinOp::Rem => if signed { BinOp::RemS } else { BinOp::RemU },
                    hir::BinOp::And => BinOp::And,
                    hir::BinOp::Or => BinOp::Or,
                    hir::BinOp::Xor => BinOp::Xor,
                    hir::BinOp::Shl => BinOp::Shl,
                    hir::BinOp::Shr => if signed { BinOp::ShrS } else { BinOp::ShrU },
                };
                let x = self.value(a);
                let mut y = self.value(b);
                if matches!(bop, BinOp::Shl | BinOp::ShrS | BinOp::ShrU) {
                    // The shift count may have another width; bring it to the
                    // left operand's width (counts are small).
                    let yt = self.types().size(b.ty);
                    let yit = self.ity(b.ty, loc);
                    if yt as u32 != it.size() {
                        y = self.conv(y, yit, it, false);
                    }
                }
                let d = self.vreg(it);
                self.f.signed[d.0 as usize] = signed;
                self.emit(Inst::Bin { op: bop, dst: d, a: x, b: y });
                Operand::Reg(d)
            }
            ExprKind::Unary(op, a) => {
                match op {
                    hir::UnOp::Not => {
                        // !x == (x == 0)
                        let at = a.ty;
                        let it = self.ity(at, loc);
                        let x = self.value(a);
                        let d = self.vreg(IrTy::I16);
                        self.emit(Inst::Cmp { cc: Cond::Eq, ty: it, dst: d, a: x, b: Operand::Imm(0) });
                        self.fit(Operand::Reg(d), IrTy::I16, ty, loc)
                    }
                    _ => {
                        let it = self.ity(ty, loc);
                        let x = self.value(a);
                        let d = self.vreg(it);
                        self.emit(Inst::Un { op: if *op == hir::UnOp::Neg { UnOp::Neg } else { UnOp::Not }, dst: d, a: x });
                        Operand::Reg(d)
                    }
                }
            }
            ExprKind::Cmp(op, a, b) => {
                let at = a.ty;
                let it = self.ity(at, loc);
                let signed = self.is_signed(at);
                let cc = cmp_cond(*op, signed);
                let x = self.value(a);
                let y = self.value(b);
                let d = self.vreg(IrTy::I16);
                self.emit(Inst::Cmp { cc, ty: it, dst: d, a: x, b: y });
                self.fit(Operand::Reg(d), IrTy::I16, ty, loc)
            }
            ExprKind::LogAnd(..) | ExprKind::LogOr(..) => {
                let it = self.ity(ty, loc);
                let d = self.vreg(it);
                let (bt, bf, end) = (self.new_block(), self.new_block(), self.new_block());
                self.cond_branch(e, bt, bf);
                self.cur = bt;
                self.emit(Inst::Mov { dst: d, src: Operand::Imm(1) });
                self.terminate(Term::Jmp(end));
                self.cur = bf;
                self.emit(Inst::Mov { dst: d, src: Operand::Imm(0) });
                self.terminate(Term::Jmp(end));
                self.cur = end;
                Operand::Reg(d)
            }
            ExprKind::Cond(c, a, b) => {
                let it = self.ity(ty, loc);
                let d = self.vreg(it);
                let (bt, bf, end) = (self.new_block(), self.new_block(), self.new_block());
                self.cond_branch(c, bt, bf);
                self.cur = bt;
                let x = self.value(a);
                self.emit(Inst::Mov { dst: d, src: x });
                self.terminate(Term::Jmp(end));
                self.cur = bf;
                let y = self.value(b);
                self.emit(Inst::Mov { dst: d, src: y });
                self.terminate(Term::Jmp(end));
                self.cur = end;
                Operand::Reg(d)
            }
            ExprKind::Comma(a, b) => {
                self.effect(a);
                self.value(b)
            }
            ExprKind::PtrAdd(..) => {
                let addr = self.ptr_addr(e);
                self.addr_value(addr)
            }
            ExprKind::PtrDiff(a, b, scale) => {
                let x = self.value(a);
                let y = self.value(b);
                let xl = self.conv(x, IrTy::Ptr, IrTy::I16, false);
                let yl = self.conv(y, IrTy::Ptr, IrTy::I16, false);
                let d = self.vreg(IrTy::I16);
                self.emit(Inst::Bin { op: BinOp::Sub, dst: d, a: xl, b: yl });
                if *scale == 1 {
                    return self.fit(Operand::Reg(d), IrTy::I16, ty, loc);
                }
                let q = self.vreg(IrTy::I16);
                self.emit(Inst::Bin { op: BinOp::DivS, dst: q, a: Operand::Reg(d), b: Operand::Imm(*scale as i64) });
                self.fit(Operand::Reg(q), IrTy::I16, ty, loc)
            }
            ExprKind::Cast(inner) => self.cast(inner, ty, loc),
            ExprKind::AddrOf(inner) => {
                if self.types().is_func(inner.ty) {
                    return self.func_addr(inner);
                }
                let lv = self.lvalue(inner);
                match lv {
                    LV::Mem(a, _, _) => self.addr_value(a),
                    LV::Reg(_) | LV::Bit(..) => {
                        self.error(loc, "cannot take this address");
                        Operand::Imm(0)
                    }
                }
            }
            ExprKind::Assign(lhs, rhs) => {
                if self.types().is_record(lhs.ty) {
                    let src = self.agg_addr(rhs);
                    let lv = self.lvalue(lhs);
                    if let LV::Mem(dst, MemTy::Agg(n), _) = lv {
                        self.emit(Inst::Memcpy { dst, src, size: n });
                    }
                    return Operand::Imm(0);
                }
                let v = self.value(rhs);
                let lv = self.lvalue(lhs);
                self.store(&lv, v.clone())
            }
            ExprKind::CompoundAssign(op, lhs, rhs, op_ty) => {
                let lv = self.lvalue(lhs);
                let lt = lhs.ty;
                let lit = self.ity(lt, loc);
                let oit = self.ity(*op_ty, loc);
                let cur = self.load(&lv);
                let cur = self.conv(cur, lit, oit, self.is_signed(lt));
                let signed = self.is_signed(*op_ty);
                let bop = match op {
                    hir::BinOp::Add => BinOp::Add,
                    hir::BinOp::Sub => BinOp::Sub,
                    hir::BinOp::Mul => BinOp::Mul,
                    hir::BinOp::Div => if signed { BinOp::DivS } else { BinOp::DivU },
                    hir::BinOp::Rem => if signed { BinOp::RemS } else { BinOp::RemU },
                    hir::BinOp::And => BinOp::And,
                    hir::BinOp::Or => BinOp::Or,
                    hir::BinOp::Xor => BinOp::Xor,
                    hir::BinOp::Shl => BinOp::Shl,
                    hir::BinOp::Shr => if signed { BinOp::ShrS } else { BinOp::ShrU },
                };
                let mut y = self.value(rhs);
                if matches!(bop, BinOp::Shl | BinOp::ShrS | BinOp::ShrU) {
                    let yit = self.ity(rhs.ty, loc);
                    if yit != oit {
                        y = self.conv(y, yit, oit, false);
                    }
                }
                let d = self.vreg(oit);
                self.emit(Inst::Bin { op: bop, dst: d, a: cur, b: y });
                let back = self.conv(Operand::Reg(d), oit, lit, signed);
                self.store(&lv, back)
            }
            ExprKind::PtrCompoundAssign(lhs, rhs, scale, sub) => {
                let lv = self.lvalue(lhs);
                let cur = self.load(&lv);
                let rit = self.ity(rhs.ty, loc);
                let i = self.value(rhs);
                let i = self.conv(i, rit, IrTy::I16, true);
                let i = if *sub {
                    let n = self.vreg(IrTy::I16);
                    self.emit(Inst::Un { op: UnOp::Neg, dst: n, a: i });
                    Operand::Reg(n)
                } else {
                    i
                };
                let addr = self.index_addr(cur, i, *scale as u32);
                let v = self.addr_value(addr);
                self.store(&lv, v)
            }
            ExprKind::IncDec(lv, inc, prefix, step) => self.incdec(lv, *inc, *prefix, *step, loc),
            ExprKind::Call(..) => match self.call(e, None) {
                Some(o) => o,
                None => Operand::Imm(0),
            },
            ExprKind::StmtExpr(stmts, last) => {
                for s in stmts {
                    self.stmt(s);
                }
                match last {
                    Some(l) => self.value(l),
                    None => Operand::Imm(0),
                }
            }
            ExprKind::VaArg(_) | ExprKind::VaStart(_) | ExprKind::VaEnd(_) => {
                self.error(loc, "variadic functions are not supported by the 65816 backend");
                Operand::Imm(0)
            }
            ExprKind::ZeroInit(_) => Operand::Imm(0),
            _ => {
                self.error(loc, "unsupported expression");
                Operand::Imm(0)
            }
        }
    }

    /// Converts a value of IR type `from` to `to` (sign-extending when
    /// `signed`).
    fn conv(&mut self, v: Operand, from: IrTy, to: IrTy, signed: bool) -> Operand {
        if from == to {
            return v;
        }
        if let Operand::Imm(x) = v {
            let x = if signed { from.sext(x) } else { from.zext(x) };
            return Operand::Imm(to.zext(x));
        }
        let kind = if to.bits() < from.bits() {
            ConvKind::Trunc
        } else if signed {
            ConvKind::Sext
        } else {
            ConvKind::Zext
        };
        let d = self.vreg(to);
        self.emit(Inst::Conv { kind, dst: d, src: v, from });
        Operand::Reg(d)
    }

    /// Fits an I16 0/1 value to the expression's declared type.
    fn fit(&mut self, v: Operand, from: IrTy, ty: Ty, loc: Loc) -> Operand {
        let it = self.ity(ty, loc);
        self.conv(v, from, it, false)
    }

    fn cast(&mut self, inner: &hir::Expr, to: Ty, loc: Loc) -> Operand {
        let from = inner.ty;
        if self.types().is_void(to) {
            self.effect(inner);
            return Operand::Imm(0);
        }
        let v = self.value(inner);
        let fi = self.ity(from, loc);
        let ti = self.ity(to, loc);
        if matches!(self.types().kind(to), TyKind::Bool) {
            if let Operand::Imm(x) = v {
                return Operand::Imm((fi.zext(x) != 0) as i64);
            }
            let d = self.vreg(IrTy::I16);
            self.emit(Inst::Cmp { cc: Cond::Ne, ty: fi, dst: d, a: v, b: Operand::Imm(0) });
            return self.conv(Operand::Reg(d), IrTy::I16, IrTy::I8, false);
        }
        match (fi, ti) {
            (IrTy::Ptr, IrTy::Ptr) => v,
            (_, IrTy::Ptr) => {
                // Integers become bank-0 addresses (hardware registers); a
                // 32-bit value keeps its bank byte.
                let signed = self.is_signed(from);
                if fi == IrTy::I8 {
                    let w = self.conv(v, IrTy::I8, IrTy::I16, signed);
                    return self.conv(w, IrTy::I16, IrTy::Ptr, false);
                }
                if fi == IrTy::I32 {
                    // Keep 24 bits.
                    if let Operand::Imm(x) = v {
                        return Operand::Imm(x & 0xff_ffff);
                    }
                    let d = self.vreg(IrTy::Ptr);
                    self.emit(Inst::Conv { kind: ConvKind::Trunc, dst: d, src: v, from: IrTy::I32 });
                    return Operand::Reg(d);
                }
                self.conv(v, fi, IrTy::Ptr, false)
            }
            (IrTy::Ptr, _) => self.conv(v, IrTy::Ptr, ti, false),
            _ => {
                let signed = self.is_signed(from);
                self.conv(v, fi, ti, signed)
            }
        }
    }

    fn func_addr(&mut self, f: &hir::Expr) -> Operand {
        match &f.kind {
            ExprKind::Global(g) => {
                let name = self.sym(*g);
                self.note_extern(*g);
                Operand::Global(name, 0)
            }
            ExprKind::Deref(p) => self.value(p),
            _ => {
                self.error(f.loc, "unsupported function designator");
                Operand::Imm(0)
            }
        }
    }

    fn note_extern(&mut self, g: GlobalId) {
        let gl = self.u.global(g);
        if !gl.is_func || gl.func.is_some() {
            return;
        }
        let name = self.sym(g);
        if self.externs.iter().any(|e| e.0 == name) {
            return;
        }
        let sig = self.types().sig(gl.ty).unwrap().clone();
        let params = sig
            .params
            .iter()
            .map(|p| match ir_ty(self.types(), *p) {
                Some(t) => ParamKind::Scalar(t),
                None => ParamKind::Aggregate(self.types().size(*p) as u32),
            })
            .collect();
        let ret = ir_ty(self.types(), sig.ret);
        self.externs.push((name, params, ret, sig.variadic || !sig.proto));
    }

    fn incdec(&mut self, lve: &hir::Expr, inc: bool, prefix: bool, step: u64, loc: Loc) -> Operand {
        let lv = self.lvalue(lve);
        let cur = self.load(&lv);
        let t = lve.ty;
        let it = self.ity(t, loc);
        // Materialise the old value for postfix use.
        let old = match &cur {
            Operand::Reg(r) if !prefix => {
                let o = self.vreg(it);
                self.emit(Inst::Mov { dst: o, src: Operand::Reg(*r) });
                Operand::Reg(o)
            }
            other => other.clone(),
        };
        let new = if it == IrTy::Ptr {
            let delta = if inc { step as i64 } else { -(step as i64) };
            let addr = match &cur {
                Operand::Reg(r) => Addr { base: Base::Reg(*r), offset: delta, index: None },
                Operand::Global(g, o) => Addr { base: Base::Global(g.clone()), offset: o + delta, index: None },
                Operand::Slot(s, o) => Addr { base: Base::Slot(*s), offset: o + delta, index: None },
                Operand::Imm(v) => Addr { base: Base::Abs(*v as u32), offset: delta, index: None },
            };
            self.addr_value(addr)
        } else {
            let d = self.vreg(it);
            self.emit(Inst::Bin { op: if inc { BinOp::Add } else { BinOp::Sub }, dst: d, a: cur, b: Operand::Imm(1) });
            Operand::Reg(d)
        };
        let stored = self.store(&lv, new);
        if prefix {
            stored
        } else {
            old
        }
    }

    // ------------------------------------------------------------------ lvalues and addresses

    fn lvalue(&mut self, e: &hir::Expr) -> LV {
        let vol = self.types().is_volatile(e.ty);
        let mem_ty = |s: &mut Self, t: Ty| -> MemTy {
            match ir_ty(s.types(), t) {
                Some(it) => MemTy::Scalar(it),
                None => MemTy::Agg(s.types().size(t) as u32),
            }
        };
        match &e.kind {
            ExprKind::Local(l) => match self.home_of(*l) {
                Home::Reg(r) => LV::Reg(r),
                Home::Slot(s) => {
                    let mt = mem_ty(self, e.ty);
                    LV::Mem(Addr { base: Base::Slot(s), offset: 0, index: None }, mt, vol)
                }
                Home::Global(g) => {
                    let mt = mem_ty(self, e.ty);
                    LV::Mem(Addr { base: Base::Global(g), offset: 0, index: None }, mt, vol)
                }
            },
            ExprKind::Global(g) | ExprKind::Str(g) => {
                let name = self.sym(*g);
                let mt = mem_ty(self, e.ty);
                LV::Mem(Addr { base: Base::Global(name), offset: 0, index: None }, mt, vol)
            }
            ExprKind::Deref(p) => {
                let a = self.ptr_addr(p);
                let mt = mem_ty(self, e.ty);
                LV::Mem(a, mt, vol)
            }
            ExprKind::Member(obj, off) => {
                let mt = mem_ty(self, e.ty);
                match self.lvalue(obj) {
                    LV::Mem(mut a, _, v) => {
                        a.offset += *off as i64;
                        LV::Mem(a, mt, v || vol)
                    }
                    other => {
                        // A scalar local accessed as a struct cannot happen
                        // (records are never registers).
                        other
                    }
                }
            }
            ExprKind::BitField(obj, off, bit, width, unit, signed) => {
                let unit_ty = match unit {
                    1 => IrTy::I8,
                    2 => IrTy::I16,
                    _ => IrTy::I32,
                };
                match self.lvalue(obj) {
                    LV::Mem(mut a, _, v) => {
                        a.offset += *off as i64;
                        LV::Bit(a, *bit, *width, unit_ty, *signed, v || vol)
                    }
                    other => other,
                }
            }
            ExprKind::CompoundLiteral(l, stmts) => {
                for s in stmts {
                    self.stmt(s);
                }
                let h = self.home_of(*l);
                let mt = mem_ty(self, e.ty);
                match h {
                    Home::Slot(s) => LV::Mem(Addr { base: Base::Slot(s), offset: 0, index: None }, mt, false),
                    Home::Reg(r) => LV::Reg(r),
                    Home::Global(g) => LV::Mem(Addr { base: Base::Global(g), offset: 0, index: None }, mt, false),
                }
            }
            _ => {
                // A non-lvalue aggregate (call result): into a temporary.
                if self.types().is_record(e.ty) {
                    let a = self.agg_addr(e);
                    let n = self.types().size(e.ty) as u32;
                    return LV::Mem(a, MemTy::Agg(n), false);
                }
                self.error(e.loc, "expression is not an lvalue");
                LV::Reg(self.vreg(IrTy::I16))
            }
        }
    }

    /// The address of an aggregate-valued expression.
    fn agg_addr(&mut self, e: &hir::Expr) -> Addr {
        match &e.kind {
            ExprKind::Call(..) => {
                let n = self.types().size(e.ty) as u32;
                let a = self.types().align(e.ty) as u32;
                let s = self.slot(n, a, "sret");
                let addr = Addr { base: Base::Slot(s), offset: 0, index: None };
                self.call(e, Some(addr.clone()));
                addr
            }
            ExprKind::Assign(lhs, _) => {
                self.value(e);
                match self.lvalue(lhs) {
                    LV::Mem(a, _, _) => a,
                    _ => Addr { base: Base::Abs(0), offset: 0, index: None },
                }
            }
            ExprKind::Cond(c, a, b) => {
                let n = self.types().size(e.ty) as u32;
                let al = self.types().align(e.ty) as u32;
                let s = self.slot(n, al, "cond");
                let dst = Addr { base: Base::Slot(s), offset: 0, index: None };
                let (bt, bf, end) = (self.new_block(), self.new_block(), self.new_block());
                self.cond_branch(c, bt, bf);
                self.cur = bt;
                let x = self.agg_addr(a);
                self.emit(Inst::Memcpy { dst: dst.clone(), src: x, size: n });
                self.terminate(Term::Jmp(end));
                self.cur = bf;
                let y = self.agg_addr(b);
                self.emit(Inst::Memcpy { dst: dst.clone(), src: y, size: n });
                self.terminate(Term::Jmp(end));
                self.cur = end;
                dst
            }
            ExprKind::Comma(a, b) => {
                self.effect(a);
                self.agg_addr(b)
            }
            ExprKind::StmtExpr(stmts, last) => {
                for s in stmts {
                    self.stmt(s);
                }
                match last {
                    Some(l) => self.agg_addr(l),
                    None => Addr { base: Base::Abs(0), offset: 0, index: None },
                }
            }
            _ => match self.lvalue(e) {
                LV::Mem(a, _, _) => a,
                _ => {
                    self.error(e.loc, "unsupported aggregate expression");
                    Addr { base: Base::Abs(0), offset: 0, index: None }
                }
            },
        }
    }

    /// The address a pointer-valued expression points to, keeping global
    /// bases and scaled indexes structured.
    fn ptr_addr(&mut self, p: &hir::Expr) -> Addr {
        match &p.kind {
            ExprKind::AddrOf(inner) if !self.types().is_func(inner.ty) => match self.lvalue(inner) {
                LV::Mem(a, _, _) => a,
                _ => {
                    self.error(p.loc, "cannot take this address");
                    Addr { base: Base::Abs(0), offset: 0, index: None }
                }
            },
            ExprKind::PtrAdd(base, idx, scale) => {
                // Indexing an array from its start: a negative index would be
                // undefined, so the index may use the 65816's indexed modes
                // (which carry into the bank byte). Anything else with a
                // signed index may legally go backwards: compute the address
                // with 16-bit arithmetic, which wraps inside the bank.
                let array_start = matches!(&base.kind, ExprKind::AddrOf(inner) if self.types().is_array(inner.ty));
                let a = self.ptr_addr(base);
                let it = self.ity(idx.ty, idx.loc);
                let signed = self.is_signed(idx.ty);
                let i = self.value(idx);
                let i = self.conv(i, it, IrTy::I16, signed);
                if signed && !array_start && i.imm().is_none() {
                    let with = self.add_index(a, i, *scale as u32);
                    let v = self.addr_value(with);
                    return self.operand_addr(v);
                }
                self.add_index(a, i, *scale as u32)
            }
            ExprKind::Cast(inner) if self.types().is_ptr(inner.ty) => self.ptr_addr(inner),
            _ => {
                let v = self.value(p);
                self.operand_addr(v)
            }
        }
    }

    fn operand_addr(&mut self, v: Operand) -> Addr {
        match v {
            Operand::Reg(r) => Addr { base: Base::Reg(r), offset: 0, index: None },
            Operand::Global(g, o) => Addr { base: Base::Global(g), offset: o, index: None },
            Operand::Slot(s, o) => Addr { base: Base::Slot(s), offset: o, index: None },
            Operand::Imm(v) => Addr { base: Base::Abs((v as u32) & 0xff_ffff), offset: 0, index: None },
        }
    }

    /// `a + i*scale`, folding constants and keeping one scaled index.
    fn add_index(&mut self, mut a: Addr, i: Operand, scale: u32) -> Addr {
        match i {
            Operand::Imm(v) => {
                a.offset += (v as i16 as i64) * scale as i64;
                a
            }
            Operand::Reg(r) => {
                if a.index.is_some() {
                    // Materialise the first index.
                    let v = self.addr_value(a);
                    let mut b = self.operand_addr(v);
                    b.index = Some((r, scale));
                    return b;
                }
                a.index = Some((r, scale));
                a
            }
            other => {
                let v = self.addr_value(a);
                let r = self.vreg(IrTy::I16);
                let t = self.conv(other, IrTy::Ptr, IrTy::I16, false);
                self.emit(Inst::Mov { dst: r, src: t });
                let mut b = self.operand_addr(v);
                b.index = Some((r, scale));
                b
            }
        }
    }

    fn index_addr(&mut self, base: Operand, i: Operand, scale: u32) -> Addr {
        let a = self.operand_addr(base);
        self.add_index(a, i, scale)
    }

    /// A pointer value for an address.
    fn addr_value(&mut self, a: Addr) -> Operand {
        if a.index.is_none() {
            match &a.base {
                Base::Global(g) => return Operand::Global(g.clone(), a.offset),
                Base::Slot(s) => return Operand::Slot(*s, a.offset),
                Base::Reg(r) if a.offset == 0 => return Operand::Reg(*r),
                Base::Abs(x) => return Operand::Imm(((*x as i64 & 0xff0000) | ((*x as i64 + a.offset) & 0xffff)) as i64),
                _ => {}
            }
        }
        let d = self.vreg(IrTy::Ptr);
        self.emit(Inst::Lea { dst: d, addr: a });
        Operand::Reg(d)
    }

    fn load(&mut self, lv: &LV) -> Operand {
        match lv {
            LV::Reg(r) => Operand::Reg(*r),
            LV::Mem(a, MemTy::Scalar(t), vol) => {
                let d = self.vreg(*t);
                self.emit(Inst::Load { dst: d, addr: a.clone(), volatile: *vol });
                Operand::Reg(d)
            }
            LV::Mem(_, MemTy::Agg(_), _) => {
                // Aggregates are handled by address; a scalar read of one is a
                // sema error.
                Operand::Imm(0)
            }
            LV::Bit(a, bit, width, ut, signed, vol) => {
                let w = self.vreg(*ut);
                self.emit(Inst::Load { dst: w, addr: a.clone(), volatile: *vol });
                // Extract: (w << (bits - bit - width)) >> (bits - width).
                let bits = ut.bits();
                let top = bits - bit - width;
                let mut v = Operand::Reg(w);
                if top > 0 {
                    let d = self.vreg(*ut);
                    self.emit(Inst::Bin { op: BinOp::Shl, dst: d, a: v, b: Operand::Imm(top as i64) });
                    v = Operand::Reg(d);
                }
                if bits - width > 0 {
                    let d = self.vreg(*ut);
                    self.emit(Inst::Bin { op: if *signed { BinOp::ShrS } else { BinOp::ShrU }, dst: d, a: v, b: Operand::Imm((bits - width) as i64) });
                    v = Operand::Reg(d);
                }
                v
            }
        }
    }

    /// Stores and returns the stored value (as the lvalue's type reads it).
    fn store(&mut self, lv: &LV, v: Operand) -> Operand {
        match lv {
            LV::Reg(r) => {
                self.emit(Inst::Mov { dst: *r, src: v });
                Operand::Reg(*r)
            }
            LV::Mem(a, MemTy::Scalar(t), vol) => {
                self.emit(Inst::Store { addr: a.clone(), src: v.clone(), ty: *t, volatile: *vol });
                v
            }
            LV::Mem(..) => v,
            LV::Bit(a, bit, width, ut, signed, vol) => {
                let old = self.vreg(*ut);
                self.emit(Inst::Load { dst: old, addr: a.clone(), volatile: *vol });
                let m = (((1u64 << width) - 1) << bit) as i64;
                let cleared = self.vreg(*ut);
                self.emit(Inst::Bin { op: BinOp::And, dst: cleared, a: Operand::Reg(old), b: Operand::Imm(ut.zext(!m)) });
                // The value arrives in the field's declared type; bring it to
                // the unit width.
                let vw = self.value_width(&v);
                let vv = self.conv(v.clone(), vw, *ut, false);
                let sh = self.vreg(*ut);
                self.emit(Inst::Bin { op: BinOp::Shl, dst: sh, a: vv, b: Operand::Imm(*bit as i64) });
                let masked = self.vreg(*ut);
                self.emit(Inst::Bin { op: BinOp::And, dst: masked, a: Operand::Reg(sh), b: Operand::Imm(m) });
                let merged = self.vreg(*ut);
                self.emit(Inst::Bin { op: BinOp::Or, dst: merged, a: Operand::Reg(cleared), b: Operand::Reg(masked) });
                self.emit(Inst::Store { addr: a.clone(), src: Operand::Reg(merged), ty: *ut, volatile: *vol });
                // The expression's value is the field's new value (truncated
                // to the width).
                let lv2 = LV::Bit(a.clone(), *bit, *width, *ut, *signed, false);
                let _ = lv2;
                let tr = self.vreg(*ut);
                let bits = ut.bits();
                self.emit(Inst::Bin { op: BinOp::Shl, dst: tr, a: Operand::Reg(masked), b: Operand::Imm((bits - bit - width) as i64) });
                let r = self.vreg(*ut);
                self.emit(Inst::Bin { op: if *signed { BinOp::ShrS } else { BinOp::ShrU }, dst: r, a: Operand::Reg(tr), b: Operand::Imm((bits - width) as i64) });
                self.conv(Operand::Reg(r), *ut, vw, *signed)
            }
        }
    }

    fn value_width(&self, v: &Operand) -> IrTy {
        match v {
            Operand::Reg(r) => self.f.ty(*r),
            Operand::Imm(_) => IrTy::I32,
            _ => IrTy::Ptr,
        }
    }

    // ------------------------------------------------------------------ calls

    fn call(&mut self, e: &hir::Expr, sret: Option<Addr>) -> Option<Operand> {
        let ExprKind::Call(callee, args) = &e.kind else { unreachable!() };
        let fty = if self.types().is_func(callee.ty) { callee.ty } else { self.types().pointee(callee.ty).unwrap() };
        let sig = self.types().sig(fty).unwrap().clone();
        let mut ops = Vec::new();
        let mut arg_tys = Vec::new();
        for a in args {
            if self.types().is_record(a.ty) {
                let addr = self.agg_addr(a);
                let v = self.addr_value(addr);
                ops.push(v);
                arg_tys.push(IrTy::Ptr);
            } else {
                let it = self.ity(a.ty, a.loc);
                ops.push(self.value(a));
                arg_tys.push(it);
            }
        }
        let c = match &callee.kind {
            ExprKind::Global(g) if self.u.global(*g).is_func => {
                self.note_extern(*g);
                Callee::Direct(self.sym(*g))
            }
            _ => {
                let v = if self.types().is_func(callee.ty) { self.func_addr(callee) } else { self.value(callee) };
                Callee::Indirect(v)
            }
        };
        let ret = ir_ty(self.types(), sig.ret);
        let is_agg = self.types().is_record(sig.ret);
        let sret = if is_agg {
            Some(sret.unwrap_or_else(|| {
                let n = self.types().size(sig.ret) as u32;
                let s = self.slot(n, 2, "sret");
                Addr { base: Base::Slot(s), offset: 0, index: None }
            }))
        } else {
            None
        };
        let dst = ret.map(|t| self.vreg(t));
        self.emit(Inst::Call { dst, callee: c, args: ops, arg_tys, sret });
        dst.map(Operand::Reg)
    }

    // ------------------------------------------------------------------ conditions

    fn cond_branch(&mut self, e: &hir::Expr, t: BlockId, f: BlockId) {
        match &e.kind {
            ExprKind::IntConst(v) => {
                self.seal_to(if *v != 0 { t } else { f });
            }
            ExprKind::LogAnd(a, b) => {
                let mid = self.new_block();
                self.cond_branch(a, mid, f);
                self.cur = mid;
                self.cond_branch(b, t, f);
            }
            ExprKind::LogOr(a, b) => {
                let mid = self.new_block();
                self.cond_branch(a, t, mid);
                self.cur = mid;
                self.cond_branch(b, t, f);
            }
            ExprKind::Unary(hir::UnOp::Not, a) => self.cond_branch(a, f, t),
            ExprKind::Cmp(op, a, b) => {
                let at = a.ty;
                let it = self.ity(at, a.loc);
                let cc = cmp_cond(*op, self.is_signed(at));
                let x = self.value(a);
                let y = self.value(b);
                self.terminate(Term::BrCmp { cc, ty: it, a: x, b: y, t, f });
            }
            ExprKind::Cast(inner) if self.types().is_integer(e.ty) && self.types().is_integer(inner.ty) && self.types().size(e.ty) >= self.types().size(inner.ty) => {
                // Widening does not change zero-ness.
                self.cond_branch(inner, t, f);
            }
            ExprKind::Comma(a, b) => {
                self.effect(a);
                self.cond_branch(b, t, f);
            }
            _ => {
                let it = self.ity(e.ty, e.loc);
                let v = self.value(e);
                self.terminate(Term::BrCmp { cc: Cond::Ne, ty: it, a: v, b: Operand::Imm(0), t, f });
            }
        }
    }

    fn seal_to(&mut self, b: BlockId) {
        self.terminate(Term::Jmp(b));
    }
}

fn cmp_cond(op: hir::CmpOp, signed: bool) -> Cond {
    match (op, signed) {
        (hir::CmpOp::Eq, _) => Cond::Eq,
        (hir::CmpOp::Ne, _) => Cond::Ne,
        (hir::CmpOp::Lt, true) => Cond::LtS,
        (hir::CmpOp::Le, true) => Cond::LeS,
        (hir::CmpOp::Gt, true) => Cond::GtS,
        (hir::CmpOp::Ge, true) => Cond::GeS,
        (hir::CmpOp::Lt, false) => Cond::LtU,
        (hir::CmpOp::Le, false) => Cond::LeU,
        (hir::CmpOp::Gt, false) => Cond::GtU,
        (hir::CmpOp::Ge, false) => Cond::GeU,
    }
}
