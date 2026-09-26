//! Instruction selection: IR to 65816, accumulator-centred.
//!
//! Every block starts and ends with A/X/Y 16-bit. A value lives in its home
//! (direct page, static frame) or, when forwarded, only in A between its
//! definition and its single use. The generator tracks what A, X and Y hold
//! to drop redundant loads.

use crate::alloc::*;
use crate::asm::{Expr, Line, Mode};
use crate::ModuleInfo;
use loomcc_ir::*;
use std::collections::BTreeSet;

#[derive(Clone, Debug, PartialEq)]
enum Val {
    /// Word `k` of a register's value.
    Reg(VReg, u32),
    Imm(i64),
}

/// A 16-bit source operand.
#[derive(Clone, Debug, PartialEq)]
enum Src {
    Imm(Expr),
    Dp(u8),
    Abs(Expr),
    Long(Expr),
    /// The value is in A (a forwarded register).
    InA,
}

/// Where a memory access goes after address preparation.
#[derive(Clone, Debug)]
enum Place {
    Abs(Expr),
    Long(Expr),
    AbsX(Expr),
    LongX(Expr),
    Dp(u8),
    /// `[dp],y` with Y holding the base offset (word k adds 2k).
    IndY(u8),
    /// `[dp]` (single word at offset 0).
    Ind(u8),
}

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Debug)]
pub enum Helper {
    Mul16,
    DivU16,
    DivS16,
    JslR10,
}

pub struct Gen<'a> {
    f: &'a Func,
    al: &'a Alloc,
    mi: &'a ModuleInfo,
    pub lines: Vec<Line>,
    frame: String,
    acc: Option<Val>,
    xv: Option<Val>,
    yv: Option<Val>,
    /// N and Z reflect A.
    flags_a: bool,
    prefix: String,
    local: u32,
    pub helpers: BTreeSet<Helper>,
    pub errors: Vec<String>,
    /// Current block index (for fall-through decisions).
    cur_block: usize,
    /// Emission order of blocks.
    order: Vec<usize>,
}

fn w(n: i64) -> Expr {
    Expr::Num(n & 0xffff)
}

impl<'a> Gen<'a> {
    pub fn new(f: &'a Func, al: &'a Alloc, mi: &'a ModuleInfo, prefix: String) -> Gen<'a> {
        Gen {
            f,
            al,
            mi,
            lines: Vec::new(),
            frame: mi.frame_symbol(&f.name),
            acc: None,
            xv: None,
            yv: None,
            flags_a: false,
            prefix,
            local: 0,
            helpers: BTreeSet::new(),
            errors: Vec::new(),
            cur_block: 0,
            order: Vec::new(),
        }
    }

    fn i(&mut self, mnem: &'static str, mode: Mode) {
        self.lines.push(Line::inst(mnem, mode));
    }

    fn i8(&mut self, mnem: &'static str, mode: Mode) {
        self.lines.push(Line::Inst { mnem, mode, wide_imm: false });
    }

    fn label(&mut self, l: String) {
        self.lines.push(Line::Label(l));
    }

    fn fresh(&mut self) -> String {
        self.local += 1;
        format!("__{}l{}", self.prefix, self.local)
    }

    fn block_label(&self, b: BlockId) -> String {
        format!("__{}b{}", self.prefix, b.0)
    }

    fn forget(&mut self) {
        self.acc = None;
        self.xv = None;
        self.yv = None;
        self.flags_a = false;
    }

    fn home(&self, r: VReg) -> Home {
        self.al.homes[r.0 as usize]
    }

    fn fwd(&self, o: &Operand) -> bool {
        matches!(o, Operand::Reg(r) if self.al.forwarded[r.0 as usize])
    }

    fn frame_expr(&self, off: u32) -> Expr {
        Expr::Sym(self.frame.clone(), off as i64)
    }

    // ------------------------------------------------------------------ operands

    /// Word `k` of an operand as a source.
    fn src(&self, o: &Operand, k: u32) -> Src {
        match o {
            Operand::Imm(v) => Src::Imm(w(v >> (16 * k))),
            Operand::Reg(r) => {
                if self.al.forwarded[r.0 as usize] {
                    return Src::InA;
                }
                match self.home(*r) {
                    Home::Dp(d) => Src::Dp(d + 2 * k as u8),
                    Home::Frame(off) => Src::Abs(self.frame_expr(off + 2 * k)),
                    Home::None => Src::Imm(Expr::Num(0)),
                }
            }
            Operand::Global(g, off) => {
                if k == 0 {
                    Src::Imm(Expr::Sym(g.clone(), *off))
                } else {
                    Src::Imm(Expr::Bank(g.clone()))
                }
            }
            Operand::Slot(s, off) => {
                if k == 0 {
                    Src::Imm(Expr::Sym(self.frame.clone(), self.al.slot_offsets[s.0 as usize] as i64 + off))
                } else {
                    Src::Imm(Expr::Num(0x7e))
                }
            }
        }
    }

    fn val_of(o: &Operand, k: u32) -> Option<Val> {
        match o {
            Operand::Reg(r) => Some(Val::Reg(*r, k)),
            Operand::Imm(v) => Some(Val::Imm((v >> (16 * k)) & 0xffff)),
            _ => None,
        }
    }

    fn op_src(&mut self, mnem: &'static str, s: &Src) {
        match s {
            Src::Imm(e) => self.i(mnem, Mode::Imm(e.clone())),
            Src::Dp(d) => self.i(mnem, Mode::Dp(*d)),
            Src::Abs(e) => self.i(mnem, Mode::Abs(e.clone())),
            Src::Long(e) => self.i(mnem, Mode::Long(e.clone())),
            Src::InA => self.errors.push(format!("{}: internal: {} with operand in A", self.f.name, mnem)),
        }
    }

    /// A = word k of the operand.
    fn lda(&mut self, o: &Operand, k: u32) {
        let v = Self::val_of(o, k);
        if v.is_some() && self.acc == v {
            return;
        }
        let s = self.src(o, k);
        if s == Src::InA {
            // Must already be in A.
            if self.acc != v {
                self.errors.push(format!("{}: internal: forwarded value not in A", self.f.name));
            }
            return;
        }
        // A copy of X or Y is cheaper than memory.
        if v.is_some() && self.xv == v {
            self.i("txa", Mode::Implied);
        } else if v.is_some() && self.yv == v {
            self.i("tya", Mode::Implied);
        } else {
            self.op_src("lda", &s);
        }
        self.acc = v;
        self.flags_a = true;
    }

    fn ldx(&mut self, o: &Operand) {
        let v = Self::val_of(o, 0);
        if v.is_some() && self.xv == v {
            return;
        }
        let s = self.src(o, 0);
        match &s {
            Src::InA => self.i("tax", Mode::Implied),
            Src::Long(_) => {
                self.lda(o, 0);
                self.i("tax", Mode::Implied);
            }
            _ => {
                if v.is_some() && self.acc == v {
                    self.i("tax", Mode::Implied);
                } else {
                    self.op_src("ldx", &s);
                }
            }
        }
        self.xv = v;
    }

    fn ldy(&mut self, o: &Operand) {
        let v = Self::val_of(o, 0);
        if v.is_some() && self.yv == v {
            return;
        }
        let s = self.src(o, 0);
        match &s {
            Src::InA => self.i("tay", Mode::Implied),
            Src::Long(_) => {
                self.lda(o, 0);
                self.i("tay", Mode::Implied);
            }
            _ => {
                if v.is_some() && self.acc == v {
                    self.i("tay", Mode::Implied);
                } else {
                    self.op_src("ldy", &s);
                }
            }
        }
        self.yv = v;
    }

    fn ldy_imm(&mut self, n: i64) {
        let v = Some(Val::Imm(n & 0xffff));
        if self.yv == v {
            return;
        }
        self.i("ldy", Mode::Imm(w(n)));
        self.yv = v;
    }

    /// Stores A to word k of register r's home (and notes A holds it).
    fn sta_reg(&mut self, r: VReg, k: u32) {
        if self.al.forwarded[r.0 as usize] {
            self.acc = Some(Val::Reg(r, k));
            return;
        }
        match self.home(r) {
            Home::Dp(d) => self.i("sta", Mode::Dp(d + 2 * k as u8)),
            Home::Frame(off) => {
                let e = self.frame_expr(off + 2 * k);
                self.i("sta", Mode::Abs(e));
            }
            Home::None => {}
        }
        // Any cached copy of the register's old value is stale.
        self.invalidate_reg(r);
        self.acc = Some(Val::Reg(r, k));
    }

    fn invalidate_reg(&mut self, r: VReg) {
        let stale = |v: &Option<Val>| matches!(v, Some(Val::Reg(x, _)) if *x == r);
        if stale(&self.acc) {
            self.acc = None;
        }
        if stale(&self.xv) {
            self.xv = None;
        }
        if stale(&self.yv) {
            self.yv = None;
        }
    }

    fn stz_reg(&mut self, r: VReg, k: u32) {
        match self.home(r) {
            Home::Dp(d) => self.i("stz", Mode::Dp(d + 2 * k as u8)),
            Home::Frame(off) => {
                let e = self.frame_expr(off + 2 * k);
                self.i("stz", Mode::Abs(e));
            }
            Home::None => {}
        }
        self.invalidate_reg(r);
    }

    fn stx_reg(&mut self, r: VReg, k: u32) {
        match self.home(r) {
            Home::Dp(d) => self.i("stx", Mode::Dp(d + 2 * k as u8)),
            Home::Frame(off) => {
                let e = self.frame_expr(off + 2 * k);
                self.i("stx", Mode::Abs(e));
            }
            Home::None => {}
        }
        self.invalidate_reg(r);
    }

    fn width(&self, t: IrTy) -> u32 {
        match t {
            IrTy::I8 | IrTy::I16 => 1,
            _ => 2,
        }
    }

    // ------------------------------------------------------------------ function

    pub fn gen_function(&mut self, abi_entry: bool) {
        let f = self.f;
        // Block order: as numbered, skipping unreachable blocks.
        let reach = reachable(f);
        self.order = (0..f.blocks.len()).filter(|&b| reach[b]).collect();
        let mi = self.mi;
        let info = mi.funcs.get(&f.name).expect("function info");
        if abi_entry {
            self.label(f.name.clone());
            self.abi_prologue();
        }
        self.label(info.body_label.clone());
        // Parameters homed in direct page.
        for (slot_off, home, ty) in self.al.param_copies.clone() {
            if let Home::Dp(d) = home {
                for k in 0..self.width(ty) {
                    let e = self.frame_expr(slot_off + 2 * k);
                    self.i("lda", Mode::Abs(e));
                    self.i("sta", Mode::Dp(d + 2 * k as u8));
                }
            }
        }
        self.forget();
        let order = self.order.clone();
        for (pos, &b) in order.iter().enumerate() {
            self.cur_block = pos;
            if b != 0 || abi_entry {
                self.label(self.block_label(BlockId(b as u32)));
            } else {
                self.label(self.block_label(BlockId(0)));
            }
            self.forget();
            let block = &f.blocks[b];
            for inst in &block.insts {
                self.gen_inst(inst);
            }
            let before = self.errors.len();
            self.gen_term(&block.term);
            if self.errors.len() > before {
                let t = print_term(&block.term);
                if let Some(e) = self.errors.last_mut() {
                    e.push_str(&format!(" (at `{}`)", t));
                }
            }
        }
    }

    fn next_block(&self) -> Option<BlockId> {
        self.order.get(self.cur_block + 1).map(|&b| BlockId(b as u32))
    }

    /// 816-tcc ABI entry: stack arguments into the parameter slots.
    fn abi_prologue(&mut self) {
        let mut so = 4u32;
        let info = self.mi.funcs.get(&self.f.name).unwrap();
        if let Some(sro) = self.al.sret_offset {
            for k in 0..2 {
                self.i("lda", Mode::Sr((so + 2 * k) as u8));
                let e = self.frame_expr(sro + 2 * k);
                self.i("sta", Mode::Abs(e));
            }
            so += 4;
        }
        let _ = info;
        for (pi, p) in self.f.params.iter().enumerate() {
            let po = self.al.param_offsets[pi];
            match p {
                ParamKind::Scalar(t) => {
                    let n = match t {
                        IrTy::I8 => 1,
                        IrTy::I16 => 2,
                        _ => 4,
                    };
                    for k in 0..self.width(*t) {
                        // An 8-bit argument is one stack byte; reading a word
                        // picks up a harmless neighbour byte.
                        self.i("lda", Mode::Sr((so + 2 * k) as u8));
                        let e = self.frame_expr(po + 2 * k);
                        self.i("sta", Mode::Abs(e));
                    }
                    so += n;
                }
                ParamKind::Aggregate(n) => {
                    let mut k = 0;
                    while k < *n {
                        self.i("lda", Mode::Sr((so + k) as u8));
                        let e = self.frame_expr(po + k);
                        self.i("sta", Mode::Abs(e));
                        k += 2;
                    }
                    so += n;
                }
            }
        }
    }

    // ------------------------------------------------------------------ instructions

    fn gen_inst(&mut self, inst: &Inst) {
        let before = self.errors.len();
        self.gen_inst_inner(inst);
        if self.errors.len() > before {
            let t = print_inst(self.f, inst);
            if let Some(e) = self.errors.last_mut() {
                e.push_str(&format!(" (at `{}`)", t));
            }
        }
    }

    fn gen_inst_inner(&mut self, inst: &Inst) {
        match inst {
            Inst::Mov { dst, src } => {
                if Some(Val::Reg(*dst, 0)) == Self::val_of(src, 0) {
                    return;
                }
                if let Operand::Reg(s) = src {
                    let same = self.home(*s) == self.home(*dst) && self.home(*dst) != Home::None && !self.al.forwarded[s.0 as usize] && !self.al.forwarded[dst.0 as usize];
                    if same {
                        // Same home: only the caches change.
                        self.invalidate_reg(*dst);
                        return;
                    }
                }
                if self.al.homes[dst.0 as usize] == Home::None && !self.al.forwarded[dst.0 as usize] {
                    return; // dead
                }
                let t = self.f.ty(*dst);
                for k in 0..self.width(t) {
                    if self.src(src, k) == Src::Imm(Expr::Num(0)) && !self.al.forwarded[dst.0 as usize] && self.home(*dst) != Home::None {
                        self.stz_reg(*dst, k);
                        continue;
                    }
                    self.lda(src, k);
                    self.sta_reg(*dst, k);
                }
            }
            Inst::Bin { op, dst, a, b } => self.gen_bin(*op, *dst, a, b),
            Inst::Un { op, dst, a } => {
                let t = self.f.ty(*dst);
                if self.dead(*dst) {
                    return;
                }
                if self.width(t) == 1 {
                    self.lda(a, 0);
                    self.i("eor", Mode::Imm(w(0xffff)));
                    if *op == UnOp::Neg {
                        self.i("inc", Mode::Acc);
                    }
                    self.acc = None;
                    self.sta_reg(*dst, 0);
                } else {
                    // Two words.
                    if *op == UnOp::Neg {
                        self.i("sec", Mode::Implied);
                        self.i("lda", Mode::Imm(w(0)));
                        let s = self.src(a, 0);
                        self.op_src("sbc", &s);
                        self.acc = None;
                        self.sta_reg(*dst, 0);
                        self.i("lda", Mode::Imm(w(0)));
                        let s = self.src(a, 1);
                        self.op_src("sbc", &s);
                        self.acc = None;
                        self.sta_reg(*dst, 1);
                    } else {
                        for k in 0..2 {
                            self.lda(a, k);
                            self.i("eor", Mode::Imm(w(0xffff)));
                            self.acc = None;
                            self.sta_reg(*dst, k);
                        }
                    }
                }
            }
            Inst::Cmp { cc, ty, dst, a, b } => {
                if self.dead(*dst) {
                    return;
                }
                let (lt, lf, end) = (self.fresh(), self.fresh(), self.fresh());
                self.cond_branch(*cc, *ty, a, b, &lt, &lf, Some(&lf));
                self.label(lf.clone());
                self.i("lda", Mode::Imm(w(0)));
                self.i("bra", Mode::Label(end.clone()));
                self.label(lt);
                self.i("lda", Mode::Imm(w(1)));
                self.label(end);
                self.forget();
                self.flags_a = true;
                let t = self.f.ty(*dst);
                self.sta_reg(*dst, 0);
                if self.width(t) == 2 {
                    self.stz_reg(*dst, 1);
                }
            }
            Inst::Conv { kind, dst, src, from } => self.gen_conv(*kind, *dst, src, *from),
            Inst::Load { dst, addr, volatile } => {
                let t = self.f.ty(*dst);
                if self.dead(*dst) && !*volatile {
                    return;
                }
                let place = self.place(addr, false);
                if t == IrTy::I8 && *volatile {
                    self.i8("sep", Mode::Imm(Expr::Num(0x20)));
                    self.mem_op("lda", &place, 0);
                    self.i8("rep", Mode::Imm(Expr::Num(0x20)));
                    self.acc = None;
                    self.flags_a = false;
                    self.sta_reg(*dst, 0);
                    return;
                }
                let overlap = match (&addr.base, self.home(*dst)) {
                    (Base::Reg(p), Home::Dp(d)) => self.home(*p) == Home::Dp(d),
                    _ => false,
                };
                if overlap && self.width(t) == 2 {
                    // dst = *dst: read both words before writing.
                    self.mem_op("lda", &place, 1);
                    self.i("tax", Mode::Implied);
                    let p0 = self.place(addr, false);
                    self.mem_op("lda", &p0, 0);
                    self.acc = None;
                    self.sta_reg(*dst, 0);
                    self.stx_reg(*dst, 1);
                    self.xv = None;
                    return;
                }
                for k in 0..self.width(t) {
                    self.mem_op("lda", &place, k);
                    self.acc = None;
                    self.flags_a = true;
                    self.sta_reg(*dst, k);
                }
            }
            Inst::Store { addr, src, ty, volatile } => self.gen_store(addr, src, *ty, *volatile),
            Inst::Lea { dst, addr } => {
                if self.dead(*dst) {
                    return;
                }
                self.gen_lea_into(addr, *dst);
            }
            Inst::Call { dst, callee, args, arg_tys, sret } => self.gen_call(*dst, callee, args, arg_tys, sret.as_ref()),
            Inst::Memcpy { dst, src, size } => self.gen_memcpy(dst, src, *size),
            Inst::Memset { dst, val, size } => self.gen_memset(dst, *val, *size),
        }
    }

    fn dead(&self, r: VReg) -> bool {
        self.al.uses[r.0 as usize] == 0
    }

    // ------------------------------------------------------------------ arithmetic

    fn gen_bin(&mut self, op: BinOp, dst: VReg, a: &Operand, b: &Operand) {
        let t = self.f.ty(dst);
        if self.dead(dst) {
            return;
        }
        if t == IrTy::Ptr {
            // Pointer arithmetic: the low word only; the bank is copied.
            let (pa, pb) = (a, b);
            self.lda(pa, 0);
            match op {
                BinOp::Add => {
                    self.i("clc", Mode::Implied);
                    let s = self.src(pb, 0);
                    self.op_src("adc", &s);
                }
                BinOp::Sub => {
                    self.i("sec", Mode::Implied);
                    let s = self.src(pb, 0);
                    self.op_src("sbc", &s);
                }
                _ => self.errors.push(format!("{}: unsupported pointer operation {:?}", self.f.name, op)),
            }
            self.acc = None;
            self.sta_reg(dst, 0);
            self.lda(pa, 1);
            self.sta_reg(dst, 1);
            return;
        }
        if t == IrTy::I32 {
            return self.gen_bin32(op, dst, a, b);
        }
        let narrow = t == IrTy::I8;
        let (mut a, mut b) = (a.clone(), b.clone());
        if self.fwd(&b) && !self.fwd(&a) && op.commutative() {
            std::mem::swap(&mut a, &mut b);
        }
        // Constant on the left of a commutative op goes right.
        if matches!(a, Operand::Imm(_)) && !matches!(b, Operand::Imm(_)) && op.commutative() && !self.fwd(&a) {
            std::mem::swap(&mut a, &mut b);
        }
        match op {
            BinOp::Add | BinOp::And | BinOp::Or | BinOp::Xor => {
                if self.fwd(&b) {
                    // Both forwarded cannot happen; b in A.
                    self.errors.push(format!("{}: internal: operand order", self.f.name));
                }
                // Operand already in X/Y with the other in A? Keep simple.
                self.lda(&a, 0);
                let bi = b.imm();
                match (op, bi) {
                    (BinOp::Add, Some(0)) | (BinOp::Or, Some(0)) | (BinOp::Xor, Some(0)) => {}
                    (BinOp::Add, Some(1)) => self.i("inc", Mode::Acc),
                    (BinOp::Add, Some(2)) => {
                        self.i("inc", Mode::Acc);
                        self.i("inc", Mode::Acc);
                    }
                    (BinOp::Add, Some(v)) if (v & 0xffff) == 0xffff => self.i("dec", Mode::Acc),
                    (BinOp::Add, Some(v)) if (v & 0xffff) == 0xfffe => {
                        self.i("dec", Mode::Acc);
                        self.i("dec", Mode::Acc);
                    }
                    (BinOp::And, Some(v)) if (v & 0xffff) == 0xffff => {}
                    (BinOp::Add, _) => {
                        self.i("clc", Mode::Implied);
                        let s = self.src(&b, 0);
                        self.op_src("adc", &s);
                    }
                    (BinOp::And, _) => {
                        let s = self.src(&b, 0);
                        self.op_src("and", &s);
                    }
                    (BinOp::Or, _) => {
                        let s = self.src(&b, 0);
                        self.op_src("ora", &s);
                    }
                    _ => {
                        let s = self.src(&b, 0);
                        self.op_src("eor", &s);
                    }
                }
            }
            BinOp::Sub => {
                if self.fwd(&b) {
                    // a - b = ~b + 1 + a
                    self.i("eor", Mode::Imm(w(0xffff)));
                    self.i("sec", Mode::Implied);
                    let s = self.src(&a, 0);
                    self.op_src("adc", &s);
                } else {
                    self.lda(&a, 0);
                    match b.imm().map(|v| v & 0xffff) {
                        Some(0) => {}
                        Some(1) => self.i("dec", Mode::Acc),
                        Some(2) => {
                            self.i("dec", Mode::Acc);
                            self.i("dec", Mode::Acc);
                        }
                        Some(0xffff) => self.i("inc", Mode::Acc),
                        _ => {
                            self.i("sec", Mode::Implied);
                            let s = self.src(&b, 0);
                            self.op_src("sbc", &s);
                        }
                    }
                }
            }
            BinOp::Shl | BinOp::ShrU | BinOp::ShrS => {
                if let Some(n) = b.imm() {
                    self.lda(&a, 0);
                    if narrow && op != BinOp::Shl {
                        self.extend8(op == BinOp::ShrS);
                    }
                    self.shift_const(op, n as u32);
                } else {
                    // Count in Y, loop.
                    if self.fwd(&b) {
                        self.i("tay", Mode::Implied);
                        self.yv = None;
                        self.acc = None;
                        self.lda(&a, 0);
                    } else {
                        self.lda(&a, 0);
                        self.ldy(&b);
                    }
                    if narrow && op != BinOp::Shl {
                        self.extend8(op == BinOp::ShrS);
                    }
                    let (lp, done) = (self.fresh(), self.fresh());
                    self.i("cpy", Mode::Imm(w(0)));
                    self.i("beq", Mode::Label(done.clone()));
                    self.label(lp.clone());
                    match op {
                        BinOp::Shl => self.i("asl", Mode::Acc),
                        BinOp::ShrU => self.i("lsr", Mode::Acc),
                        _ => {
                            self.i("cmp", Mode::Imm(w(0x8000)));
                            self.i("ror", Mode::Acc);
                        }
                    }
                    self.i("dey", Mode::Implied);
                    self.i("bne", Mode::Label(lp));
                    self.label(done);
                    self.yv = None;
                }
            }
            BinOp::Mul => {
                if let Some(k) = b.imm() {
                    self.mul_const(&a, k & 0xffff);
                } else if let Some(k) = a.imm() {
                    self.mul_const(&b, k & 0xffff);
                } else {
                    self.load_a_x(&a, &b);
                    self.call_helper(Helper::Mul16);
                }
            }
            BinOp::DivU | BinOp::RemU | BinOp::DivS | BinOp::RemS => {
                let signed = matches!(op, BinOp::DivS | BinOp::RemS);
                let rem = matches!(op, BinOp::RemU | BinOp::RemS);
                if let (Some(k), false) = (b.imm().map(|v| v & 0xffff), signed) {
                    if k != 0 && k & (k - 1) == 0 {
                        self.lda(&a, 0);
                        if narrow {
                            self.extend8(false);
                        }
                        if rem {
                            self.i("and", Mode::Imm(w(k - 1)));
                        } else {
                            self.shift_const(BinOp::ShrU, k.trailing_zeros());
                        }
                        self.acc = None;
                        self.sta_reg(dst, 0);
                        return;
                    }
                }
                if let (Some(k), true, false) = (b.imm().map(|v| v & 0xffff), signed, rem) {
                    if k > 1 && k < 0x8000 && k & (k - 1) == 0 {
                        // Round toward zero: add k-1 to negatives, then shift.
                        self.lda(&a, 0);
                        if narrow {
                            self.extend8(true);
                        }
                        let skip = self.fresh();
                        self.i("cmp", Mode::Imm(w(0)));
                        self.i("bpl", Mode::Label(skip.clone()));
                        self.i("clc", Mode::Implied);
                        self.i("adc", Mode::Imm(w(k - 1)));
                        self.label(skip);
                        self.shift_const(BinOp::ShrS, k.trailing_zeros());
                        self.acc = None;
                        self.sta_reg(dst, 0);
                        return;
                    }
                }
                if narrow {
                    // Extend both operands to 16 bits in scratch.
                    self.lda(&b, 0);
                    self.extend8(signed);
                    self.i("tax", Mode::Implied);
                    self.acc = None;
                    self.lda(&a, 0);
                    self.extend8(signed);
                } else {
                    self.load_a_x(&a, &b);
                }
                self.call_helper(if signed { Helper::DivS16 } else { Helper::DivU16 });
                if rem {
                    self.i("txa", Mode::Implied);
                }
            }
        }
        self.acc = None;
        self.flags_a = !matches!(op, BinOp::Mul | BinOp::DivU | BinOp::DivS | BinOp::RemU | BinOp::RemS);
        self.sta_reg(dst, 0);
    }

    /// A = a, X = b (for helpers).
    fn load_a_x(&mut self, a: &Operand, b: &Operand) {
        if self.fwd(b) {
            self.i("tax", Mode::Implied);
            self.xv = None;
            self.acc = None;
            self.lda(a, 0);
        } else {
            self.lda(a, 0);
            self.ldx(b);
        }
    }

    fn extend8(&mut self, signed: bool) {
        self.i("and", Mode::Imm(w(0xff)));
        if signed {
            self.i("eor", Mode::Imm(w(0x80)));
            self.i("sec", Mode::Implied);
            self.i("sbc", Mode::Imm(w(0x80)));
        }
        self.acc = None;
    }

    fn shift_const(&mut self, op: BinOp, n: u32) {
        let n = n.min(16);
        match op {
            BinOp::Shl => {
                if n >= 16 {
                    self.i("lda", Mode::Imm(w(0)));
                    return;
                }
                let mut n = n;
                if n >= 8 {
                    self.i("xba", Mode::Implied);
                    self.i("and", Mode::Imm(w(0xff00)));
                    n -= 8;
                }
                for _ in 0..n {
                    self.i("asl", Mode::Acc);
                }
            }
            BinOp::ShrU => {
                if n >= 16 {
                    self.i("lda", Mode::Imm(w(0)));
                    return;
                }
                let mut n = n;
                if n >= 8 {
                    self.i("xba", Mode::Implied);
                    self.i("and", Mode::Imm(w(0x00ff)));
                    n -= 8;
                }
                for _ in 0..n {
                    self.i("lsr", Mode::Acc);
                }
            }
            _ => {
                if n >= 15 {
                    // All sign bits.
                    self.i("asl", Mode::Acc);
                    self.i("lda", Mode::Imm(w(0)));
                    self.i("sbc", Mode::Imm(w(0)));
                    self.i("eor", Mode::Imm(w(0xffff)));
                    return;
                }
                let mut n = n;
                if n >= 8 {
                    // xba, sign-extend the low byte.
                    self.i("xba", Mode::Implied);
                    self.i("and", Mode::Imm(w(0x00ff)));
                    self.i("eor", Mode::Imm(w(0x0080)));
                    self.i("sec", Mode::Implied);
                    self.i("sbc", Mode::Imm(w(0x0080)));
                    n -= 8;
                }
                for _ in 0..n {
                    self.i("cmp", Mode::Imm(w(0x8000)));
                    self.i("ror", Mode::Acc);
                }
            }
        }
    }

    /// A = a * k by shifts and adds (Horner over k's bits).
    fn mul_const(&mut self, a: &Operand, k: i64) {
        let k = k & 0xffff;
        if k == 0 {
            self.i("lda", Mode::Imm(w(0)));
            return;
        }
        let neg = k > 0x8000 && (0x10000 - k).count_ones() + 1 < k.count_ones();
        let m = if neg { 0x10000 - k } else { k };
        self.lda(a, 0);
        if m.count_ones() == 1 {
            self.shift_const(BinOp::Shl, m.trailing_zeros());
        } else {
            // The multiplicand must be addressable for the adds.
            let s = match self.src(a, 0) {
                Src::InA | Src::Long(_) => {
                    self.i("sta", Mode::Dp(SCRATCH_WORD));
                    Src::Dp(SCRATCH_WORD)
                }
                s => s,
            };
            let top = 63 - (m as u64).leading_zeros();
            for bit in (0..top).rev() {
                self.i("asl", Mode::Acc);
                if m >> bit & 1 != 0 {
                    self.i("clc", Mode::Implied);
                    self.op_src("adc", &s);
                }
            }
        }
        if neg {
            self.i("eor", Mode::Imm(w(0xffff)));
            self.i("inc", Mode::Acc);
        }
        self.acc = None;
    }

    fn gen_bin32(&mut self, op: BinOp, dst: VReg, a: &Operand, b: &Operand) {
        match op {
            BinOp::Add | BinOp::Sub => {
                self.i(if op == BinOp::Add { "clc" } else { "sec" }, Mode::Implied);
                for k in 0..2 {
                    self.acc = None;
                    let s = self.src(a, k);
                    self.op_src("lda", &s);
                    let s = self.src(b, k);
                    self.op_src(if op == BinOp::Add { "adc" } else { "sbc" }, &s);
                    self.acc = None;
                    self.sta_reg(dst, k);
                }
            }
            BinOp::And | BinOp::Or | BinOp::Xor => {
                let m = match op {
                    BinOp::And => "and",
                    BinOp::Or => "ora",
                    _ => "eor",
                };
                for k in 0..2 {
                    self.lda(a, k);
                    let s = self.src(b, k);
                    self.op_src(m, &s);
                    self.acc = None;
                    self.sta_reg(dst, k);
                }
            }
            BinOp::Shl | BinOp::ShrU | BinOp::ShrS if b.imm().is_some() => {
                // Copy then shift in place n times.
                let n = b.imm().unwrap().clamp(0, 32) as u32;
                for k in 0..2 {
                    self.lda(a, k);
                    self.sta_reg(dst, k);
                }
                let (lo, hi) = match (self.src(&Operand::Reg(dst), 0), self.src(&Operand::Reg(dst), 1)) {
                    (Src::Dp(x), Src::Dp(y)) => (Mode::Dp(x), Mode::Dp(y)),
                    (Src::Abs(x), Src::Abs(y)) => (Mode::Abs(x), Mode::Abs(y)),
                    _ => {
                        self.errors.push(format!("{}: 32-bit shift destination", self.f.name));
                        return;
                    }
                };
                for _ in 0..n {
                    match op {
                        BinOp::Shl => {
                            self.i("asl", lo.clone());
                            self.i("rol", hi.clone());
                        }
                        BinOp::ShrU => {
                            self.i("lsr", hi.clone());
                            self.i("ror", lo.clone());
                        }
                        _ => {
                            self.i("lda", hi.clone());
                            self.i("cmp", Mode::Imm(w(0x8000)));
                            self.i("ror", hi.clone());
                            self.i("ror", lo.clone());
                        }
                    }
                }
                self.acc = None;
                self.invalidate_reg(dst);
            }
            _ => self.errors.push(format!("{}: 32-bit {:?} is not supported yet", self.f.name, op)),
        }
    }

    fn gen_conv(&mut self, kind: ConvKind, dst: VReg, src: &Operand, from: IrTy) {
        if self.dead(dst) {
            return;
        }
        let to = self.f.ty(dst);
        match kind {
            ConvKind::Trunc => {
                self.lda(src, 0);
                self.sta_reg(dst, 0);
                if to == IrTy::Ptr {
                    self.lda(src, 1);
                    self.i("and", Mode::Imm(w(0xff)));
                    self.acc = None;
                    self.sta_reg(dst, 1);
                }
            }
            ConvKind::Zext => {
                self.lda(src, 0);
                if from == IrTy::I8 {
                    self.i("and", Mode::Imm(w(0xff)));
                    self.acc = None;
                }
                if from == IrTy::I32 || from == IrTy::Ptr {
                    // i32 <-> ptr: copy both words.
                    self.sta_reg(dst, 0);
                    self.lda(src, 1);
                    if to == IrTy::Ptr {
                        self.i("and", Mode::Imm(w(0xff)));
                        self.acc = None;
                    }
                    self.sta_reg(dst, 1);
                    return;
                }
                self.sta_reg(dst, 0);
                if self.width(to) == 2 {
                    self.stz_reg(dst, 1);
                }
            }
            ConvKind::Sext => {
                self.lda(src, 0);
                if from == IrTy::I8 {
                    self.extend8(true);
                }
                if from == IrTy::I32 || from == IrTy::Ptr {
                    self.sta_reg(dst, 0);
                    self.lda(src, 1);
                    self.sta_reg(dst, 1);
                    return;
                }
                self.sta_reg(dst, 0);
                if self.width(to) == 2 {
                    // High word: all copies of the sign bit.
                    self.i("asl", Mode::Acc);
                    self.i("lda", Mode::Imm(w(0)));
                    self.i("sbc", Mode::Imm(w(0)));
                    self.i("eor", Mode::Imm(w(0xffff)));
                    self.acc = None;
                    self.sta_reg(dst, 1);
                }
            }
        }
    }

    // ------------------------------------------------------------------ memory

    fn global_is_near(&self, g: &str) -> bool {
        self.mi.near_globals.contains(g)
    }

    /// Emits what an access to `addr` needs (index into X or Y, pointer
    /// staging) and returns where the words are. `keep_a`: A holds a value
    /// that must survive (it is saved in scratch and reloaded).
    fn place(&mut self, addr: &Addr, keep_a: bool) -> Place {
        let saved = if keep_a && self.place_needs_a(addr) {
            self.i("sta", Mode::Dp(SAVE_A));
            Some(self.acc.clone())
        } else {
            None
        };
        let p = self.place_inner(addr);
        if let Some(v) = saved {
            self.i("lda", Mode::Dp(SAVE_A));
            self.acc = v;
            self.flags_a = true;
        }
        p
    }

    fn place_needs_a(&self, addr: &Addr) -> bool {
        let base_needs = match &addr.base {
            Base::Reg(p) => !matches!(self.home(*p), Home::Dp(_)) || addr.offset < 0 || addr.offset >= 0x8000,
            _ => false,
        };
        let idx_needs = match addr.index {
            Some((r, s)) => {
                s != 1 || matches!(addr.base, Base::Reg(_)) && addr.offset != 0 || self.al.forwarded[r.0 as usize] || matches!(self.src(&Operand::Reg(r), 0), Src::Long(_))
            }
            None => false,
        };
        base_needs || idx_needs
    }

    /// Computes index*scale (+ add) into A.
    fn scaled_index_in_a(&mut self, r: VReg, scale: u32, add: i64) {
        self.lda(&Operand::Reg(r), 0);
        if scale.is_power_of_two() {
            self.shift_const(BinOp::Shl, scale.trailing_zeros());
        } else {
            let op = if self.al.forwarded[r.0 as usize] {
                // Value only in A: multiply from A.
                Operand::Reg(r)
            } else {
                Operand::Reg(r)
            };
            self.acc = Some(Val::Reg(r, 0));
            self.mul_const(&op, scale as i64);
        }
        if add != 0 {
            if add & 0xffff == 1 {
                self.i("inc", Mode::Acc);
            } else {
                self.i("clc", Mode::Implied);
                self.i("adc", Mode::Imm(w(add)));
            }
        }
        self.acc = None;
    }

    fn place_inner(&mut self, addr: &Addr) -> Place {
        match &addr.base {
            Base::Global(_) | Base::Abs(_) | Base::Slot(_) => {
                let (expr, near) = match &addr.base {
                    Base::Global(g2) => (Expr::Sym(g2.clone(), addr.offset), self.global_is_near(g2)),
                    Base::Slot(s) => (Expr::Sym(self.frame.clone(), self.al.slot_offsets[s.0 as usize] as i64 + addr.offset), true),
                    Base::Abs(a) => {
                        let full = super::interp_add(*a, addr.offset);
                        if full >> 16 == 0x7e {
                            (Expr::Num((full & 0xffff) as i64), true)
                        } else {
                            (Expr::Num(full as i64), false)
                        }
                    }
                    _ => unreachable!(),
                };
                match addr.index {
                    None => {
                        if near {
                            Place::Abs(expr)
                        } else {
                            Place::Long(expr)
                        }
                    }
                    Some((r, s)) => {
                        if s == 1 && !self.al.forwarded[r.0 as usize] {
                            self.ldx(&Operand::Reg(r));
                        } else {
                            self.scaled_index_in_a(r, s, 0);
                            self.i("tax", Mode::Implied);
                            self.xv = None;
                        }
                        if near {
                            Place::AbsX(expr)
                        } else {
                            Place::LongX(expr)
                        }
                    }
                }
            }
            Base::Reg(p) if addr.offset < 0 || addr.offset >= 0x8000 => {
                // A negative offset must wrap in the bank: add it in 16 bits
                // into a scratch pointer (Y would carry into the bank byte).
                let idx_in_a = addr.index.map_or(false, |(r, _)| self.al.forwarded[r.0 as usize]);
                let held = self.acc.clone();
                if idx_in_a {
                    self.i("tax", Mode::Implied);
                }
                self.acc = None;
                let s0 = self.src(&Operand::Reg(*p), 0);
                self.op_src("lda", &s0);
                self.i("clc", Mode::Implied);
                self.i("adc", Mode::Imm(w(addr.offset)));
                self.i("sta", Mode::Dp(SCRATCH_PTR));
                let s1 = self.src(&Operand::Reg(*p), 1);
                self.op_src("lda", &s1);
                self.i("sta", Mode::Dp(SCRATCH_PTR + 2));
                if idx_in_a {
                    self.i("txa", Mode::Implied);
                    self.acc = held;
                    self.xv = None;
                }
                match addr.index {
                    None => Place::Ind(SCRATCH_PTR),
                    Some((r, s)) => {
                        if s == 1 && !self.al.forwarded[r.0 as usize] {
                            self.ldy(&Operand::Reg(r));
                        } else {
                            self.scaled_index_in_a(r, s, 0);
                            self.i("tay", Mode::Implied);
                            self.yv = None;
                        }
                        Place::IndY(SCRATCH_PTR)
                    }
                }
            }
            Base::Reg(p) => {
                let dp = match self.home(*p) {
                    Home::Dp(d) => d,
                    _ => {
                        // Stage the pointer in scratch direct page, keeping a
                        // forwarded index (in A) in X meanwhile.
                        let idx_in_a = addr.index.map_or(false, |(r, _)| self.al.forwarded[r.0 as usize]);
                        let held = self.acc.clone();
                        if idx_in_a {
                            self.i("tax", Mode::Implied);
                        }
                        for k in 0..2 {
                            self.acc = None;
                            let s = self.src(&Operand::Reg(*p), k);
                            self.op_src("lda", &s);
                            self.i("sta", Mode::Dp(SCRATCH_PTR + 2 * k as u8));
                        }
                        self.acc = None;
                        if idx_in_a {
                            self.i("txa", Mode::Implied);
                            self.acc = held;
                            self.xv = None;
                        }
                        SCRATCH_PTR
                    }
                };
                match addr.index {
                    None => {
                        if addr.offset == 0 {
                            Place::Ind(dp)
                        } else {
                            self.ldy_imm(addr.offset);
                            Place::IndY(dp)
                        }
                    }
                    Some((r, s)) => {
                        if s == 1 && addr.offset == 0 && !self.al.forwarded[r.0 as usize] {
                            self.ldy(&Operand::Reg(r));
                        } else {
                            self.scaled_index_in_a(r, s, addr.offset);
                            self.i("tay", Mode::Implied);
                            self.yv = None;
                        }
                        Place::IndY(dp)
                    }
                }
            }
        }
    }

    /// `mnem` on word k of a place. For IndY, word k > 0 advances Y first
    /// (callers go through words in order).
    fn mem_op(&mut self, mnem: &'static str, p: &Place, k: u32) {
        let add = |e: &Expr, k: u32| -> Expr {
            match e {
                Expr::Sym(s, o) => Expr::Sym(s.clone(), o + 2 * k as i64),
                Expr::Num(n) => Expr::Num(n + 2 * k as i64),
                other => other.clone(),
            }
        };
        match p {
            Place::Abs(e) => self.i(mnem, Mode::Abs(add(e, k))),
            Place::Long(e) => {
                if mnem == "stz" {
                    self.i("lda", Mode::Imm(w(0)));
                    self.acc = Some(Val::Imm(0));
                    self.i("sta", Mode::Long(add(e, k)));
                } else {
                    self.i(mnem, Mode::Long(add(e, k)));
                }
            }
            Place::AbsX(e) => self.i(mnem, Mode::AbsX(add(e, k))),
            Place::LongX(e) => {
                if mnem == "stz" {
                    self.i("lda", Mode::Imm(w(0)));
                    self.acc = Some(Val::Imm(0));
                    self.i("sta", Mode::LongX(add(e, k)));
                } else {
                    self.i(mnem, Mode::LongX(add(e, k)));
                }
            }
            Place::Dp(d) => self.i(mnem, Mode::Dp(d + 2 * k as u8)),
            Place::IndY(d) => {
                if k > 0 {
                    self.i("iny", Mode::Implied);
                    self.i("iny", Mode::Implied);
                    self.yv = None;
                }
                if mnem == "stz" {
                    self.i("lda", Mode::Imm(w(0)));
                    self.acc = Some(Val::Imm(0));
                    self.i("sta", Mode::DpIndLongY(*d));
                } else {
                    self.i(mnem, Mode::DpIndLongY(*d));
                }
            }
            Place::Ind(d) => {
                if k > 0 {
                    self.ldy_imm(2 * k as i64);
                    if mnem == "stz" {
                        self.i("lda", Mode::Imm(w(0)));
                        self.acc = Some(Val::Imm(0));
                        self.i("sta", Mode::DpIndLongY(*d));
                    } else {
                        self.i(mnem, Mode::DpIndLongY(*d));
                    }
                } else if mnem == "stz" {
                    self.i("lda", Mode::Imm(w(0)));
                    self.acc = Some(Val::Imm(0));
                    self.i("sta", Mode::DpIndLong(*d));
                } else {
                    self.i(mnem, Mode::DpIndLong(*d));
                }
            }
        }
    }

    fn gen_store(&mut self, addr: &Addr, src: &Operand, ty: IrTy, volatile: bool) {
        let src_in_a = self.fwd(src);
        let place = self.place(addr, src_in_a);
        let n = self.width(ty);
        if ty == IrTy::I8 {
            if src.imm() == Some(0) && !matches!(place, Place::Long(_) | Place::LongX(_) | Place::IndY(_) | Place::Ind(_)) {
                self.i8("sep", Mode::Imm(Expr::Num(0x20)));
                self.mem_op("stz", &place, 0);
                self.i8("rep", Mode::Imm(Expr::Num(0x20)));
                return;
            }
            self.lda(src, 0);
            self.i8("sep", Mode::Imm(Expr::Num(0x20)));
            self.mem_op("sta", &place, 0);
            self.i8("rep", Mode::Imm(Expr::Num(0x20)));
            let _ = volatile;
            return;
        }
        for k in 0..n {
            if self.src(src, k) == Src::Imm(Expr::Num(0)) && !src_in_a {
                self.mem_op("stz", &place, k);
                continue;
            }
            self.lda(src, k);
            self.mem_op("sta", &place, k);
        }
    }

    /// dst (ptr) = address.
    fn gen_lea_into(&mut self, addr: &Addr, dst: VReg) {
        // Low word.
        match &addr.base {
            Base::Reg(p) => {
                match addr.index {
                    Some((r, s)) => {
                        self.scaled_index_in_a(r, s, addr.offset);
                        self.i("clc", Mode::Implied);
                        let sp = self.src(&Operand::Reg(*p), 0);
                        self.op_src("adc", &sp);
                    }
                    None => {
                        self.lda(&Operand::Reg(*p), 0);
                        if addr.offset != 0 {
                            self.i("clc", Mode::Implied);
                            self.i("adc", Mode::Imm(w(addr.offset)));
                        }
                    }
                }
                self.acc = None;
                self.sta_reg(dst, 0);
                self.lda(&Operand::Reg(*p), 1);
                self.sta_reg(dst, 1);
            }
            other => {
                let (lo, bank) = match other {
                    Base::Global(g) => (Expr::Sym(g.clone(), addr.offset), Expr::Bank(g.clone())),
                    Base::Slot(s) => (Expr::Sym(self.frame.clone(), self.al.slot_offsets[s.0 as usize] as i64 + addr.offset), Expr::Num(0x7e)),
                    Base::Abs(a) => {
                        let full = super::interp_add(*a, addr.offset);
                        (Expr::Num((full & 0xffff) as i64), Expr::Num((full >> 16) as i64))
                    }
                    Base::Reg(_) => unreachable!(),
                };
                match addr.index {
                    Some((r, s)) => {
                        self.scaled_index_in_a(r, s, 0);
                        self.i("clc", Mode::Implied);
                        self.i("adc", Mode::Imm(lo));
                    }
                    None => self.i("lda", Mode::Imm(lo)),
                }
                self.acc = None;
                self.sta_reg(dst, 0);
                self.i("lda", Mode::Imm(bank));
                self.acc = None;
                self.sta_reg(dst, 1);
            }
        }
    }

    /// Materialises an address into a direct-page pointer (4 bytes at `dp`).
    fn addr_to_dp(&mut self, addr: &Addr, dp: u8) {
        match &addr.base {
            Base::Reg(p) => {
                match addr.index {
                    Some((r, s)) => {
                        self.scaled_index_in_a(r, s, addr.offset);
                        self.i("clc", Mode::Implied);
                        let sp = self.src(&Operand::Reg(*p), 0);
                        self.op_src("adc", &sp);
                    }
                    None => {
                        self.acc = None;
                        let sp = self.src(&Operand::Reg(*p), 0);
                        self.op_src("lda", &sp);
                        if addr.offset != 0 {
                            self.i("clc", Mode::Implied);
                            self.i("adc", Mode::Imm(w(addr.offset)));
                        }
                    }
                }
                self.i("sta", Mode::Dp(dp));
                let sp = self.src(&Operand::Reg(*p), 1);
                self.op_src("lda", &sp);
                self.i("sta", Mode::Dp(dp + 2));
            }
            other => {
                let (lo, bank) = match other {
                    Base::Global(g) => (Expr::Sym(g.clone(), addr.offset), Expr::Bank(g.clone())),
                    Base::Slot(s) => (Expr::Sym(self.frame.clone(), self.al.slot_offsets[s.0 as usize] as i64 + addr.offset), Expr::Num(0x7e)),
                    Base::Abs(a) => {
                        let full = super::interp_add(*a, addr.offset);
                        (Expr::Num((full & 0xffff) as i64), Expr::Num((full >> 16) as i64))
                    }
                    Base::Reg(_) => unreachable!(),
                };
                match addr.index {
                    Some((r, s)) => {
                        self.scaled_index_in_a(r, s, 0);
                        self.i("clc", Mode::Implied);
                        self.i("adc", Mode::Imm(lo));
                    }
                    None => self.i("lda", Mode::Imm(lo)),
                }
                self.i("sta", Mode::Dp(dp));
                self.i("lda", Mode::Imm(bank));
                self.i("sta", Mode::Dp(dp + 2));
            }
        }
        self.acc = None;
        self.flags_a = false;
    }

    fn gen_memcpy(&mut self, dst: &Addr, src: &Addr, size: u32) {
        self.addr_to_dp(src, SCRATCH_PTR2);
        self.addr_to_dp(dst, SCRATCH_PTR);
        self.copy_loop(size, true);
    }

    fn gen_memset(&mut self, dst: &Addr, val: u8, size: u32) {
        self.addr_to_dp(dst, SCRATCH_PTR);
        let v = (val as i64) | ((val as i64) << 8);
        self.i("lda", Mode::Imm(w(v)));
        self.copy_loop(size, false);
    }

    /// Copies (or fills with A) `size` bytes to [$1c] from [$18].
    fn copy_loop(&mut self, size: u32, copy: bool) {
        let words = size / 2;
        if words > 0 {
            if words <= 4 {
                for k in 0..words {
                    self.i("ldy", Mode::Imm(w((2 * k) as i64)));
                    if copy {
                        self.i("lda", Mode::DpIndLongY(SCRATCH_PTR2));
                    }
                    self.i("sta", Mode::DpIndLongY(SCRATCH_PTR));
                }
            } else {
                let lp = self.fresh();
                self.i("ldy", Mode::Imm(w((2 * (words - 1)) as i64)));
                self.label(lp.clone());
                if copy {
                    self.i("lda", Mode::DpIndLongY(SCRATCH_PTR2));
                }
                self.i("sta", Mode::DpIndLongY(SCRATCH_PTR));
                self.i("dey", Mode::Implied);
                self.i("dey", Mode::Implied);
                self.i("bpl", Mode::Label(lp));
            }
        }
        if size % 2 == 1 {
            self.i("ldy", Mode::Imm(w((size - 1) as i64)));
            self.i8("sep", Mode::Imm(Expr::Num(0x20)));
            if copy {
                self.i("lda", Mode::DpIndLongY(SCRATCH_PTR2));
            }
            self.i("sta", Mode::DpIndLongY(SCRATCH_PTR));
            self.i8("rep", Mode::Imm(Expr::Num(0x20)));
        }
        self.forget();
    }

    // ------------------------------------------------------------------ calls

    fn call_helper(&mut self, h: Helper) {
        self.helpers.insert(h);
        let name = self.mi.helper_name(h);
        self.i("jsl", Mode::Label(name));
        self.xv = None;
        self.yv = None;
        self.acc = None;
    }

    fn gen_call(&mut self, dst: Option<VReg>, callee: &Callee, args: &[Operand], arg_tys: &[IrTy], sret: Option<&Addr>) {
        let internal = match callee {
            Callee::Direct(n) => self.mi.funcs.get(n).cloned(),
            Callee::Indirect(_) => None,
        };
        if let Some(info) = internal {
            // Arguments straight into the callee's parameter slots.
            for (i, a) in args.iter().enumerate() {
                let Some(p) = info.params.get(i) else { break };
                let po = info.param_offsets[i];
                match p {
                    ParamKind::Scalar(t) => {
                        for k in 0..self.width(*t) {
                            let e = Expr::Sym(info.frame_sym.clone(), (po + 2 * k) as i64);
                            if self.src(a, k) == Src::Imm(Expr::Num(0)) {
                                self.i("stz", Mode::Abs(e));
                            } else {
                                self.lda(a, k);
                                self.i("sta", Mode::Abs(e));
                            }
                        }
                    }
                    ParamKind::Aggregate(n) => {
                        let addr = match a {
                            Operand::Reg(r) => Addr { base: Base::Reg(*r), offset: 0, index: None },
                            Operand::Global(g, o) => Addr { base: Base::Global(g.clone()), offset: *o, index: None },
                            Operand::Slot(s, o) => Addr { base: Base::Slot(*s), offset: *o, index: None },
                            Operand::Imm(v) => Addr { base: Base::Abs(*v as u32), offset: 0, index: None },
                        };
                        self.addr_to_dp(&addr, SCRATCH_PTR2);
                        let mut k = 0;
                        while k < *n {
                            self.i("ldy", Mode::Imm(w(k as i64)));
                            self.i("lda", Mode::DpIndLongY(SCRATCH_PTR2));
                            self.i("sta", Mode::Abs(Expr::Sym(info.frame_sym.clone(), (po + k) as i64)));
                            k += 2;
                        }
                        self.forget();
                    }
                }
            }
            if let (Some(sa), Some(so)) = (sret, info.sret_offset) {
                self.addr_to_dp(sa, SCRATCH_PTR);
                for k in 0..2 {
                    self.i("lda", Mode::Dp(SCRATCH_PTR + 2 * k as u8));
                    self.i("sta", Mode::Abs(Expr::Sym(info.frame_sym.clone(), (so + 2 * k) as i64)));
                }
            }
            self.i("jsl", Mode::Label(info.body_label.clone()));
            self.forget();
            if let Some(d) = dst {
                let t = self.f.ty(d);
                self.acc = None;
                self.sta_reg_after_call(d, 0);
                if self.width(t) == 2 {
                    self.stx_reg(d, 1);
                }
                self.acc = Some(Val::Reg(d, 0));
                self.flags_a = false;
            }
            return;
        }
        // 816-tcc ABI: push right to left; a struct goes on the stack whole.
        let kinds: Vec<ParamKind> = match callee {
            Callee::Direct(n) => self.mi.externs.get(n).cloned().unwrap_or_default(),
            Callee::Indirect(_) => Vec::new(),
        };
        let mut pushed = 0u32;
        for (i, a) in args.iter().enumerate().rev() {
            if let Some(ParamKind::Aggregate(n)) = kinds.get(i) {
                let n = *n;
                let addr = match a {
                    Operand::Reg(r) => Addr { base: Base::Reg(*r), offset: 0, index: None },
                    Operand::Global(g, o) => Addr { base: Base::Global(g.clone()), offset: *o, index: None },
                    Operand::Slot(sl, o) => Addr { base: Base::Slot(*sl), offset: *o, index: None },
                    Operand::Imm(v) => Addr { base: Base::Abs(*v as u32), offset: 0, index: None },
                };
                self.addr_to_dp(&addr, SCRATCH_PTR2);
                self.i("tsc", Mode::Implied);
                self.i("sec", Mode::Implied);
                self.i("sbc", Mode::Imm(w(n as i64)));
                self.i("tcs", Mode::Implied);
                let mut k = 0;
                while k < n {
                    self.i("ldy", Mode::Imm(w(k as i64)));
                    if n - k == 1 {
                        self.i8("sep", Mode::Imm(Expr::Num(0x20)));
                        self.i("lda", Mode::DpIndLongY(SCRATCH_PTR2));
                        self.i("sta", Mode::Sr((k + 1) as u8));
                        self.i8("rep", Mode::Imm(Expr::Num(0x20)));
                    } else {
                        self.i("lda", Mode::DpIndLongY(SCRATCH_PTR2));
                        self.i("sta", Mode::Sr((k + 1) as u8));
                    }
                    k += 2;
                }
                self.forget();
                pushed += n;
                continue;
            }
            let t = arg_tys.get(i).copied().unwrap_or(IrTy::I16);
            match t {
                IrTy::I8 => {
                    self.lda(a, 0);
                    self.i8("sep", Mode::Imm(Expr::Num(0x20)));
                    self.i("pha", Mode::Implied);
                    self.i8("rep", Mode::Imm(Expr::Num(0x20)));
                    pushed += 1;
                }
                _ => {
                    for k in (0..self.width(t)).rev() {
                        match self.src(a, k) {
                            Src::Imm(e) => self.i("pea", Mode::Abs(e)),
                            Src::Dp(d) => self.i("pei", Mode::DpInd(d)),
                            _ => {
                                self.lda(a, k);
                                self.i("pha", Mode::Implied);
                            }
                        }
                        pushed += 2;
                    }
                }
            }
        }
        if let Some(sa) = sret {
            self.addr_to_dp(sa, SCRATCH_PTR2);
            self.i("pei", Mode::DpInd(SCRATCH_PTR2 + 2));
            self.i("pei", Mode::DpInd(SCRATCH_PTR2));
            pushed += 4;
        }
        match callee {
            Callee::Direct(n) => self.i("jsl", Mode::Label(n.clone())),
            Callee::Indirect(o) => {
                for k in 0..2 {
                    self.acc = None;
                    let s = self.src(o, k);
                    if s == Src::InA {
                        self.errors.push("indirect call target in A".into());
                    }
                    self.op_src("lda", &s);
                    self.i("sta", Mode::Dp(SCRATCH_PTR + 2 * k as u8));
                }
                self.helpers.insert(Helper::JslR10);
                let name = self.mi.helper_name(Helper::JslR10);
                self.i("jsl", Mode::Label(name));
            }
        }
        self.forget();
        // Pop the arguments.
        match pushed {
            0 => {}
            2 => self.i("ply", Mode::Implied),
            4 => {
                self.i("ply", Mode::Implied);
                self.i("ply", Mode::Implied);
            }
            n => {
                self.i("tsc", Mode::Implied);
                self.i("clc", Mode::Implied);
                self.i("adc", Mode::Imm(w(n as i64)));
                self.i("tcs", Mode::Implied);
            }
        }
        self.yv = None;
        self.acc = None;
        if let Some(d) = dst {
            let t = self.f.ty(d);
            for k in 0..self.width(t) {
                self.i("lda", Mode::Dp(2 * k as u8));
                self.acc = None;
                self.sta_reg(d, k);
            }
            self.flags_a = true;
        }
    }

    fn sta_reg_after_call(&mut self, d: VReg, k: u32) {
        self.sta_reg(d, k);
    }

    // ------------------------------------------------------------------ terminators

    fn jump(&mut self, b: BlockId) {
        if self.next_block() == Some(b) {
            return;
        }
        let l = self.block_label(b);
        self.i("bra", Mode::Label(l));
    }

    fn gen_term(&mut self, t: &Term) {
        match t {
            Term::Jmp(b) => self.jump(*b),
            Term::Br { cond, t, f } => {
                let ty = match cond {
                    Operand::Reg(r) => self.f.ty(*r),
                    _ => IrTy::I16,
                };
                self.branch(Cond::Ne, ty, cond, &Operand::Imm(0), *t, *f);
            }
            Term::BrCmp { cc, ty, a, b, t, f } => self.branch(*cc, *ty, a, b, *t, *f),
            Term::Switch { val, ty, cases, default } => self.gen_switch(val, *ty, cases, *default),
            Term::Ret(v) => {
                let exported = self.mi.funcs.get(&self.f.name).map_or(false, |i| i.has_abi_entry);
                if let Some(v) = v {
                    let t = self.f.ret.unwrap_or(IrTy::I16);
                    if self.width(t) == 2 {
                        match self.src(v, 1) {
                            Src::Imm(e) => self.i("ldx", Mode::Imm(e)),
                            Src::Dp(d) => self.i("ldx", Mode::Dp(d)),
                            Src::Abs(e) => self.i("ldx", Mode::Abs(e)),
                            _ => {
                                self.lda(v, 1);
                                self.i("tax", Mode::Implied);
                            }
                        }
                        if exported {
                            self.i("stx", Mode::Dp(2));
                        }
                    }
                    self.lda(v, 0);
                    if t == IrTy::I8 {
                        // Callers through the 816-tcc ABI read the low byte;
                        // internal callers only the low byte too.
                    }
                    if exported {
                        self.i("sta", Mode::Dp(0));
                    }
                }
                self.i("rtl", Mode::Implied);
            }
            Term::Unreachable => {
                self.i("rtl", Mode::Implied);
            }
        }
    }

    /// Branches to t if `a cc b`, else to f, falling through when possible.
    fn branch(&mut self, cc: Cond, ty: IrTy, a: &Operand, b: &Operand, t: BlockId, f: BlockId) {
        let next = self.next_block();
        let (lt, lf) = (self.block_label(t), self.block_label(f));
        if next == Some(t) {
            // Branch to f when the condition fails.
            let nl = lf.clone();
            self.cond_branch(cc.negate(), ty, a, b, &nl, &lt, Some(&lt));
        } else {
            self.cond_branch(cc, ty, a, b, &lt, &lf, if next == Some(f) { Some(&lf) } else { None });
            if next != Some(f) {
                self.i("bra", Mode::Label(lf));
            }
        }
    }

    /// Emits a compare that jumps to `lt` when `a cc b`; otherwise falls to
    /// the following code (which the caller arranges to be `lf`). `fall` is
    /// the label that follows, if known.
    fn cond_branch(&mut self, cc: Cond, ty: IrTy, a: &Operand, b: &Operand, lt: &str, lf: &str, _fall: Option<&str>) {
        let lt = lt.to_string();
        let lf = lf.to_string();
        match ty {
            IrTy::Ptr | IrTy::I32 => return self.cond_branch_wide(cc, ty, a, b, &lt, &lf),
            _ => {}
        }
        let (mut a, mut b, mut cc) = (a.clone(), b.clone(), cc);
        if self.fwd(&b) {
            std::mem::swap(&mut a, &mut b);
            cc = cc.swap();
        }
        // Constants on the left go right.
        if a.imm().is_some() && b.imm().is_none() {
            std::mem::swap(&mut a, &mut b);
            cc = cc.swap();
        }
        let eight = ty == IrTy::I8;
        let bi = b.imm().map(|v| if eight { v & 0xff } else { v & 0xffff });
        // Compare with zero: flags from a load.
        if bi == Some(0) && !eight {
            let v = Self::val_of(&a, 0);
            if !(self.acc == v && v.is_some() && self.flags_a) {
                if self.acc == v && v.is_some() || self.fwd(&a) {
                    // In A but flags stale.
                    self.i("cmp", Mode::Imm(w(0)));
                } else {
                    self.acc = None;
                    self.lda(&a, 0);
                }
            }
            match cc {
                Cond::Eq | Cond::LeU => self.i("beq", Mode::Label(lt)),
                Cond::Ne | Cond::GtU => self.i("bne", Mode::Label(lt)),
                Cond::LtS => self.i("bmi", Mode::Label(lt)),
                Cond::GeS => self.i("bpl", Mode::Label(lt)),
                Cond::GtS => {
                    self.i("beq", Mode::Label(lf));
                    self.i("bpl", Mode::Label(lt));
                }
                Cond::LeS => {
                    self.i("beq", Mode::Label(lt.clone()));
                    self.i("bmi", Mode::Label(lt));
                }
                Cond::LtU => {} // never
                Cond::GeU => self.i("bra", Mode::Label(lt)),
            }
            return;
        }
        if eight && matches!(cc, Cond::GtS | Cond::LeS) {
            // Normalise to lt/ge: a > k == a >= k+1; otherwise swap operands.
            match bi {
                Some(k) if (k as i8) < 127 => {
                    let k1 = ((k as i8) as i64 + 1) & 0xff;
                    let c2 = if cc == Cond::GtS { Cond::GeS } else { Cond::LtS };
                    return self.cond_branch(c2, ty, &a, &Operand::Imm(k1), &lt, &lf, None);
                }
                Some(_) => {
                    // a > 127 never; a <= 127 always.
                    if cc == Cond::LeS {
                        self.i("bra", Mode::Label(lt));
                    }
                    return;
                }
                None => {
                    if self.fwd(&a) {
                        self.i("sta", Mode::Dp(SCRATCH_WORD));
                        self.acc = None;
                        self.lda(&b, 0);
                        self.i8("sep", Mode::Imm(Expr::Num(0x20)));
                        self.i("sec", Mode::Implied);
                        self.i("sbc", Mode::Dp(SCRATCH_WORD));
                        let skip = self.fresh();
                        self.i("bvc", Mode::Label(skip.clone()));
                        self.i8("eor", Mode::Imm(Expr::Num(0x80)));
                        self.label(skip);
                        self.i8("rep", Mode::Imm(Expr::Num(0x20)));
                        self.acc = None;
                        self.flags_a = false;
                        self.emit_cc_branch(if cc == Cond::GtS { Cond::LtS } else { Cond::GeS }, &lt, &lf);
                        return;
                    }
                    return self.cond_branch(cc.swap(), ty, &b, &a, &lt, &lf, None);
                }
            }
        }
        if eight {
            self.lda(&a, 0);
            self.i8("sep", Mode::Imm(Expr::Num(0x20)));
            if cc.is_signed() {
                self.signed_cmp8(&b);
            } else {
                match self.src(&b, 0) {
                    Src::Imm(Expr::Num(n)) => self.i8("cmp", Mode::Imm(Expr::Num(n & 0xff))),
                    s => self.op_src("cmp", &s),
                }
            }
            self.i8("rep", Mode::Imm(Expr::Num(0x20)));
            self.flags_a = false;
            self.emit_cc_branch(cc, &lt, &lf);
            return;
        }
        if cc.is_signed() {
            if let Some(k) = bi {
                // Flip the sign bits and compare unsigned.
                self.lda(&a, 0);
                self.i("eor", Mode::Imm(w(0x8000)));
                self.acc = None;
                self.i("cmp", Mode::Imm(w(k ^ 0x8000)));
                self.flags_a = false;
                let ucc = match cc {
                    Cond::LtS => Cond::LtU,
                    Cond::LeS => Cond::LeU,
                    Cond::GtS => Cond::GtU,
                    _ => Cond::GeU,
                };
                self.emit_cc_branch(ucc, &lt, &lf);
                return;
            }
            // General: a - b with the overflow fix; Gt/Le swap operands when
            // `a` is not already in A.
            let (x, y, c2) = if matches!(cc, Cond::GtS | Cond::LeS) && !self.fwd(&a) {
                (b.clone(), a.clone(), cc.swap())
            } else {
                (a.clone(), b.clone(), cc)
            };
            if matches!(c2, Cond::GtS | Cond::LeS) {
                // a in A, compare against memory: stash and swap.
                self.lda(&x, 0);
                self.i("sta", Mode::Dp(SCRATCH_WORD));
                self.acc = None;
                self.lda(&y, 0);
                self.i("sec", Mode::Implied);
                self.i("sbc", Mode::Dp(SCRATCH_WORD));
                self.signed_fix_and_branch(c2.swap(), &lt, &lf);
                return;
            }
            self.lda(&x, 0);
            self.i("sec", Mode::Implied);
            let s = self.src(&y, 0);
            self.op_src("sbc", &s);
            self.signed_fix_and_branch(c2, &lt, &lf);
            return;
        }
        // Unsigned and equality.
        self.lda(&a, 0);
        if let (Some(k), Cond::GtU | Cond::LeU) = (bi, cc) {
            if k < 0xffff {
                self.i("cmp", Mode::Imm(w(k + 1)));
                self.flags_a = false;
                self.emit_cc_branch(if cc == Cond::GtU { Cond::GeU } else { Cond::LtU }, &lt, &lf);
                return;
            }
        }
        let s = self.src(&b, 0);
        self.op_src("cmp", &s);
        self.flags_a = false;
        self.emit_cc_branch(cc, &lt, &lf);
    }

    fn signed_cmp8(&mut self, b: &Operand) {
        // In 8-bit mode: A - b with overflow fix-up; leaves N meaning "less".
        self.i("sec", Mode::Implied);
        match self.src(b, 0) {
            Src::Imm(Expr::Num(n)) => self.i8("sbc", Mode::Imm(Expr::Num(n & 0xff))),
            s => self.op_src("sbc", &s),
        }
        let skip = self.fresh();
        self.i("bvc", Mode::Label(skip.clone()));
        self.i8("eor", Mode::Imm(Expr::Num(0x80)));
        self.label(skip);
        self.acc = None;
    }

    /// After `a - b` in A: N = (a < b) signed; Z = (a == b) before the fix.
    fn signed_fix_and_branch(&mut self, cc: Cond, lt: &str, lf: &str) {
        self.acc = None;
        self.flags_a = false;
        match cc {
            Cond::LtS | Cond::GeS => {
                let skip = self.fresh();
                self.i("bvc", Mode::Label(skip.clone()));
                self.i("eor", Mode::Imm(w(0x8000)));
                self.label(skip);
                self.i(if cc == Cond::LtS { "bmi" } else { "bpl" }, Mode::Label(lt.to_string()));
            }
            Cond::GtS | Cond::LeS => {
                // Needs Z too: a == b first.
                let skip = self.fresh();
                if cc == Cond::GtS {
                    self.i("beq", Mode::Label(lf.to_string()));
                } else {
                    self.i("beq", Mode::Label(lt.to_string()));
                }
                self.i("bvc", Mode::Label(skip.clone()));
                self.i("eor", Mode::Imm(w(0x8000)));
                self.label(skip);
                self.i(if cc == Cond::GtS { "bpl" } else { "bmi" }, Mode::Label(lt.to_string()));
            }
            _ => {}
        }
    }

    /// Branch on the flags of a `cmp` (unsigned/equality semantics; signed
    /// conditions here mean the 8-bit path already folded the sign).
    fn emit_cc_branch(&mut self, cc: Cond, lt: &str, lf: &str) {
        let lt = lt.to_string();
        match cc {
            Cond::Eq => self.i("beq", Mode::Label(lt)),
            Cond::Ne => self.i("bne", Mode::Label(lt)),
            Cond::LtU => self.i("bcc", Mode::Label(lt)),
            Cond::GeU => self.i("bcs", Mode::Label(lt)),
            Cond::GtU => {
                self.i("beq", Mode::Label(lf.to_string()));
                self.i("bcs", Mode::Label(lt));
            }
            Cond::LeU => {
                self.i("bcc", Mode::Label(lt.clone()));
                self.i("beq", Mode::Label(lt));
            }
            // 8-bit signed: after sbc/bvc/eor, N = less.
            Cond::LtS => self.i("bmi", Mode::Label(lt)),
            Cond::GeS => self.i("bpl", Mode::Label(lt)),
            Cond::GtS | Cond::LeS => {
                // 8-bit signed Gt/Le: fall back to Lt/Ge with swapped sense
                // is not possible here; handled by the caller swapping.
                self.errors.push("8-bit signed gt/le".into());
            }
        }
    }

    fn cond_branch_wide(&mut self, cc: Cond, ty: IrTy, a: &Operand, b: &Operand, lt: &str, lf: &str) {
        let lt = lt.to_string();
        let lf = lf.to_string();
        let zero = b.imm() == Some(0);
        if matches!(cc, Cond::Eq | Cond::Ne) {
            if zero {
                self.lda(a, 0);
                let s = self.src(a, 1);
                if ty == IrTy::Ptr {
                    // The bank byte only (the high byte of the word is not
                    // part of the address).
                    self.acc = None;
                    self.i("sta", Mode::Dp(SCRATCH_WORD));
                    self.op_src("lda", &s);
                    self.i("and", Mode::Imm(w(0xff)));
                    self.i("ora", Mode::Dp(SCRATCH_WORD));
                } else {
                    self.op_src("ora", &s);
                }
                self.acc = None;
                self.i(if cc == Cond::Eq { "beq" } else { "bne" }, Mode::Label(lt));
                return;
            }
            self.lda(a, 0);
            let s = self.src(b, 0);
            self.op_src("cmp", &s);
            self.i("bne", Mode::Label(if cc == Cond::Eq { lf.clone() } else { lt.clone() }));
            self.acc = None;
            let s = self.src(a, 1);
            self.op_src("lda", &s);
            if ty == IrTy::Ptr {
                self.i("eor", Mode::Imm(w(0)));
            }
            let s = self.src(b, 1);
            self.op_src("cmp", &s);
            self.i(if cc == Cond::Eq { "beq" } else { "bne" }, Mode::Label(lt));
            return;
        }
        if ty == IrTy::Ptr {
            // Relational pointer compares: low word, unsigned.
            let c = match cc {
                Cond::LtS => Cond::LtU,
                Cond::LeS => Cond::LeU,
                Cond::GtS => Cond::GtU,
                Cond::GeS => Cond::GeU,
                c => c,
            };
            return self.cond_branch(c, IrTy::I16, a, b, &lt, &lf, None);
        }
        // 32-bit relational: a full subtract; Gt/Le by swapping operands.
        let (x, y, c) = match cc {
            Cond::GtS | Cond::LeS | Cond::GtU | Cond::LeU => (b.clone(), a.clone(), cc.swap()),
            _ => (a.clone(), b.clone(), cc),
        };
        self.acc = None;
        let s = self.src(&x, 0);
        self.op_src("lda", &s);
        let s = self.src(&y, 0);
        self.op_src("cmp", &s);
        let s = self.src(&x, 1);
        self.op_src("lda", &s);
        let s = self.src(&y, 1);
        self.op_src("sbc", &s);
        self.acc = None;
        self.flags_a = false;
        match c {
            Cond::LtU => self.i("bcc", Mode::Label(lt)),
            Cond::GeU => self.i("bcs", Mode::Label(lt)),
            Cond::LtS | Cond::GeS => {
                let skip = self.fresh();
                self.i("bvc", Mode::Label(skip.clone()));
                self.i("eor", Mode::Imm(w(0x8000)));
                self.label(skip);
                self.i(if c == Cond::LtS { "bmi" } else { "bpl" }, Mode::Label(lt));
            }
            _ => unreachable!(),
        }
    }

    fn gen_switch(&mut self, val: &Operand, ty: IrTy, cases: &[(i64, BlockId)], default: BlockId) {
        let mut cs: Vec<(i64, BlockId)> = cases.iter().map(|(v, b)| (ty.zext(*v), *b)).collect();
        cs.sort();
        let ld = self.block_label(default);
        if cs.is_empty() {
            self.jump(default);
            return;
        }
        let (min, max) = (cs[0].0, cs[cs.len() - 1].0);
        let span = max - min + 1;
        self.lda(val, 0);
        if ty == IrTy::I8 {
            self.i("and", Mode::Imm(w(0xff)));
            self.acc = None;
        }
        if cs.len() >= 4 && span <= 3 * cs.len() as i64 + 4 && span < 256 {
            // Jump table.
            if min != 0 {
                self.i("sec", Mode::Implied);
                self.i("sbc", Mode::Imm(w(min)));
            }
            self.i("cmp", Mode::Imm(w(span)));
            self.i("bcc", Mode::Label(format!("{}_in", ld)));
            self.i("brl", Mode::Label(ld.clone()));
            self.label(format!("{}_in", ld));
            let table = self.fresh();
            self.i("asl", Mode::Acc);
            self.i("tax", Mode::Implied);
            self.i("jmp", Mode::AbsIndX(Expr::Sym(table.clone(), 0)));
            self.label(table);
            let mut entries = Vec::new();
            for v in min..=max {
                let b = cs.iter().find(|c| c.0 == v).map(|c| c.1).unwrap_or(default);
                entries.push(self.block_label(b));
            }
            // Table labels are code-bank addresses (jmp (abs,x) reads the
            // program bank).
            for chunk in entries.chunks(8) {
                self.lines.push(Line::Data(format!("  .dw {}", chunk.join(", ")), 2 * chunk.len() as u32));
            }
            self.forget();
            return;
        }
        for (v, b) in &cs {
            self.i("cmp", Mode::Imm(w(*v)));
            let l = self.block_label(*b);
            self.i("beq", Mode::Label(l));
        }
        self.flags_a = false;
        self.jump(default);
    }
}

pub fn reachable(f: &Func) -> Vec<bool> {
    let mut seen = vec![false; f.blocks.len()];
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
    seen
}
