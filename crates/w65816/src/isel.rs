//! Instruction selection: IR to 65816, accumulator-centred.
//!
//! Every block starts and ends with A/X/Y 16-bit. A value lives in its home
//! (direct page, static frame) or, when forwarded, only in A between its
//! definition and its single use. The generator tracks what A, X and Y hold
//! to drop redundant loads.

use crate::alloc::*;

/// Where an index-register value is parked for an operation that needs it in
/// memory.
const XY_SPILL: u8 = 0x18;
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
    /// The value lives in an index register.
    X,
    Y,
    /// A folded load: read from its address in place.
    Mem(VReg),
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
    /// Software versions for interrupt context (the multiplier and divider
    /// registers are not saved by the NMI handler).
    Mul16Soft,
    DivU16Soft,
    DivS16Soft,
    /// 32-bit operations (0 mul, 1 divu, 2 divs, 3 remu, 4 rems); the flag
    /// selects the copy with interrupt-private work RAM.
    A32(u8, bool),
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
    /// The CPU's accumulator is 8-bit at this point of the emitted code.
    m8: bool,
    /// Inside an explicit 8-bit section.
    in8: bool,
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
            m8: false,
            in8: false,
        }
    }

    /// Enters an explicit 8-bit accumulator section (emits `sep #$20` only
    /// when the CPU is in 16-bit mode).
    fn begin8(&mut self) {
        if !self.m8 {
            self.lines.push(Line::Inst { mnem: "sep", mode: Mode::Imm(Expr::Num(0x20)), wide_imm: false });
            self.m8 = true;
        }
        self.in8 = true;
    }

    /// Leaves the section; the switch back to 16-bit is emitted lazily, before
    /// the next instruction that depends on the accumulator width, a branch,
    /// a label or a call.
    fn end8(&mut self) {
        self.in8 = false;
    }

    fn ensure16(&mut self) {
        if self.m8 && !self.in8 {
            self.lines.push(Line::Inst { mnem: "rep", mode: Mode::Imm(Expr::Num(0x20)), wide_imm: false });
            self.m8 = false;
        }
    }

    fn i(&mut self, mnem: &'static str, mode: Mode) {
        let index_only = matches!(mnem, "ldx" | "ldy" | "stx" | "sty" | "inx" | "iny" | "dex" | "dey" | "cpx" | "cpy" | "txy" | "tyx" | "clc" | "sec" | "nop" | "phx" | "phy" | "plx" | "ply");
        if self.m8 && !self.in8 && !index_only {
            self.ensure16();
        }
        if self.in8 && !index_only {
            if let Mode::Imm(Expr::Num(n)) = mode {
                if !matches!(mnem, "sep" | "rep") {
                    self.lines.push(Line::Inst { mnem, mode: Mode::Imm(Expr::Num(n & 0xff)), wide_imm: false });
                    return;
                }
            }
            if let Mode::Imm(_) = mode {
                if !matches!(mnem, "sep" | "rep") {
                    self.errors.push(format!("internal: symbolic immediate for {} in 8-bit mode", mnem));
                }
            }
        }
        // N and Z stop reflecting A after an instruction that sets them from
        // something else.
        if matches!(mnem, "ldx" | "ldy" | "inx" | "iny" | "dex" | "dey" | "cpx" | "cpy" | "tax" | "tay" | "txy" | "tyx" | "plx" | "ply" | "cmp" | "bit" | "inc" | "dec" | "asl" | "lsr" | "rol" | "ror" | "tsb" | "trb")
            && !matches!(mode, Mode::Acc)
        {
            self.flags_a = false;
        }
        self.lines.push(Line::inst(mnem, mode));
    }

    fn i8(&mut self, mnem: &'static str, mode: Mode) {
        if self.m8 && !self.in8 && !matches!(mnem, "sep" | "rep") {
            self.ensure16();
        }
        self.lines.push(Line::Inst { mnem, mode, wide_imm: false });
    }

    fn label(&mut self, l: String) {
        if !self.in8 {
            self.ensure16();
        }
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
                if self.al.folded[r.0 as usize].is_some() {
                    return Src::Mem(*r);
                }
                match self.home(*r) {
                    Home::Dp(d) => Src::Dp(d + 2 * k as u8),
                    Home::Frame(off) => Src::Abs(self.frame_expr(off + 2 * k)),
                    Home::X => Src::X,
                    Home::Y => Src::Y,
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
            Src::Mem(r) => {
                let addr = self.al.folded[r.0 as usize].clone().unwrap();
                let keep = mnem != "lda";
                // A carry set up for adc/sbc must survive the address
                // preparation (which may shift): move it after.
                let carry = match self.lines.last() {
                    Some(Line::Inst { mnem: m @ ("clc" | "sec"), .. }) if matches!(mnem, "adc" | "sbc") => Some(*m),
                    _ => None,
                };
                if carry.is_some() {
                    self.lines.pop();
                }
                let p = self.place(&addr, keep);
                if let Some(c) = carry {
                    self.i(c, Mode::Implied);
                }
                self.mem_op(mnem, &p, 0);
            }
            Src::X | Src::Y => {
                // Through the scratch word.
                self.i(if *s == Src::X { "stx" } else { "sty" }, Mode::Dp(XY_SPILL));
                self.i(mnem, Mode::Dp(XY_SPILL));
            }
        }
    }

    /// A = word k of the operand.
    fn lda(&mut self, o: &Operand, k: u32) {
        let v = Self::val_of(o, k);
        if v.is_some() && self.acc == v {
            return;
        }
        let s = self.src(o, k);
        if s == Src::X || s == Src::Y {
            self.i(if s == Src::X { "txa" } else { "tya" }, Mode::Implied);
            self.acc = v;
            self.flags_a = true;
            return;
        }
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
            Src::X => return,
            Src::Y => self.i("tyx", Mode::Implied),
            Src::InA => self.i("tax", Mode::Implied),
            Src::Long(_) | Src::Mem(_) => {
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
            Src::Y => return,
            Src::X => self.i("txy", Mode::Implied),
            Src::InA => self.i("tay", Mode::Implied),
            Src::Long(_) | Src::Mem(_) => {
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
            Home::X => self.i("tax", Mode::Implied),
            Home::Y => self.i("tay", Mode::Implied),
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
            Home::X => self.i("ldx", Mode::Imm(w(0))),
            Home::Y => self.i("ldy", Mode::Imm(w(0))),
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
            Home::X => {}
            Home::Y => self.i("txy", Mode::Implied),
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
        self.order = layout(f, &reach);
        let mi = self.mi;
        let info = mi.funcs.get(&f.name).expect("function info");
        if abi_entry {
            self.label(f.name.clone());
            self.abi_prologue();
        }
        self.label(info.body_label.clone());
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
        if let Some(r) = self.f.sret_reg {
            let h = self.home(r);
            for k in 0..2 {
                self.i("lda", Mode::Sr((so + 2 * k) as u8));
                self.a_to_home(h, k);
            }
            so += 4;
        }
        for (pi, p) in self.f.params.iter().enumerate() {
            let po = self.al.param_offsets[pi];
            match p {
                ParamKind::Scalar(t) => {
                    let n = match t {
                        IrTy::I8 => 1,
                        IrTy::I16 => 2,
                        _ => 4,
                    };
                    let h = self.f.param_regs[pi].map(|r| self.home(r)).unwrap_or(Home::None);
                    for k in 0..self.width(*t) {
                        if h == Home::None {
                            break;
                        }
                        // An 8-bit argument is one stack byte; reading a word
                        // picks up a harmless neighbour byte.
                        self.i("lda", Mode::Sr((so + 2 * k) as u8));
                        self.a_to_home(h, k);
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

    /// Stores A into word k of a home in this function's frame/DP/X/Y.
    fn a_to_home(&mut self, h: Home, k: u32) {
        match h {
            Home::Dp(d) => self.i("sta", Mode::Dp(d + 2 * k as u8)),
            Home::Frame(o) => {
                let e = self.frame_expr(o + 2 * k);
                self.i("sta", Mode::Abs(e));
            }
            Home::X => self.i("tax", Mode::Implied),
            Home::Y => self.i("tay", Mode::Implied),
            Home::None => {}
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
                if matches!(self.home(*dst), Home::X | Home::Y) && !self.fwd(src) {
                    match self.src(src, 0) {
                        Src::Imm(e) | Src::Abs(e) | Src::Long(e) if !matches!(self.src(src, 0), Src::Long(_)) => {
                            let m = if self.home(*dst) == Home::X { "ldx" } else { "ldy" };
                            match self.src(src, 0) {
                                Src::Imm(_) => self.i(m, Mode::Imm(e)),
                                _ => self.i(m, Mode::Abs(e)),
                            }
                            self.invalidate_reg(*dst);
                            return;
                        }
                        Src::Dp(d) => {
                            let m = if self.home(*dst) == Home::X { "ldx" } else { "ldy" };
                            self.i(m, Mode::Dp(d));
                            self.invalidate_reg(*dst);
                            return;
                        }
                        _ => {}
                    }
                }
                if let (Src::X | Src::Y, Home::Dp(d)) = (self.src(src, 0), self.home(*dst)) {
                    let m = if self.src(src, 0) == Src::X { "stx" } else { "sty" };
                    self.i(m, Mode::Dp(d));
                    self.invalidate_reg(*dst);
                    return;
                }
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
                // Branch-free forms: the carry of a compare is the answer.
                let narrow = matches!(ty, IrTy::I16) || (*ty == IrTy::I8 && self.is_clean8(a) && (b.imm().is_some() || self.is_clean8(b)));
                let bzero = b.imm().map(|v| v & 0xffff) == Some(0);
                if narrow && !self.fwd(b) && (matches!(cc, Cond::Eq | Cond::Ne) && bzero || matches!(cc, Cond::LtU | Cond::GeU)) {
                    self.lda(a, 0);
                    if bzero && matches!(cc, Cond::Eq | Cond::Ne) {
                        self.i("cmp", Mode::Imm(w(1)));
                    } else {
                        let sb = self.src(b, 0);
                        self.op_src("cmp", &sb);
                    }
                    // C = (a >= b) unsigned; for `!= 0`, C = (a >= 1).
                    self.i("lda", Mode::Imm(w(0)));
                    self.i("rol", Mode::Acc);
                    if matches!(cc, Cond::Eq | Cond::LtU) {
                        self.i("eor", Mode::Imm(w(1)));
                    }
                    self.acc = None;
                    self.flags_a = true;
                    let t = self.f.ty(*dst);
                    self.sta_reg(*dst, 0);
                    if self.width(t) == 2 {
                        self.stz_reg(*dst, 1);
                    }
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
                if (self.dead(*dst) && !*volatile) || self.al.folded[dst.0 as usize].is_some() {
                    return;
                }
                let place = self.place(addr, false);
                if t == IrTy::I8 && *volatile {
                    self.begin8();
                    self.mem_op("lda", &place, 0);
                    self.end8();
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

    /// An 8-bit value whose 16-bit home has a zero high byte.
    fn is_clean8(&self, o: &Operand) -> bool {
        match o {
            Operand::Reg(r) => self.al.clean8[r.0 as usize],
            Operand::Imm(v) => (v & 0xffff) < 0x100,
            _ => false,
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
            let same_home = matches!(pa, Operand::Reg(r) if self.home(*r) == self.home(dst) && self.home(dst) != Home::None);
            if !same_home {
                self.lda(pa, 1);
                self.sta_reg(dst, 1);
            }
            return;
        }
        if t == IrTy::I32 {
            return self.gen_bin32(op, dst, a, b);
        }
        let narrow = t == IrTy::I8;
        // v = v +/- small constant with v in X or Y: inx/iny, dex/dey.
        if let (Home::X | Home::Y, Operand::Reg(x), Some(k)) = (self.home(dst), a, b.imm()) {
            if *x == dst && matches!(op, BinOp::Add | BinOp::Sub) {
                let k = k & 0xffff;
                let delta: i64 = if op == BinOp::Add { if k >= 0x8000 { k - 0x10000 } else { k } } else if k >= 0x8000 { 0x10000 - k } else { -k };
                if delta.abs() <= 3 {
                    let xr = self.home(dst) == Home::X;
                    let m = match (xr, delta > 0) {
                        (true, true) => "inx",
                        (true, false) => "dex",
                        (false, true) => "iny",
                        (false, false) => "dey",
                    };
                    for _ in 0..delta.abs() {
                        self.i(m, Mode::Implied);
                    }
                    self.invalidate_reg(dst);
                    return;
                }
            }
        }
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
                Src::InA | Src::Long(_) | Src::X | Src::Y | Src::Mem(_) => {
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
            BinOp::Shl | BinOp::ShrU | BinOp::ShrS => {
                // Variable count: copy, then shift in place Y times.
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
                self.acc = None;
                let s0 = self.src(b, 0);
                self.op_src("lda", &s0);
                self.i("and", Mode::Imm(w(0x3f)));
                self.i("tay", Mode::Implied);
                let (lp, done) = (self.fresh(), self.fresh());
                self.i("beq", Mode::Label(done.clone()));
                self.label(lp.clone());
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
                self.i("dey", Mode::Implied);
                self.i("bne", Mode::Label(lp));
                self.label(done);
                self.acc = None;
                self.yv = None;
                self.invalidate_reg(dst);
            }
            BinOp::Mul | BinOp::DivU | BinOp::DivS | BinOp::RemU | BinOp::RemS => {
                for k in 0..2u32 {
                    self.acc = None;
                    let s = self.src(a, k);
                    self.op_src("lda", &s);
                    self.i("sta", Mode::Dp(0x18 + 2 * k as u8));
                }
                for k in 0..2u32 {
                    self.acc = None;
                    let s = self.src(b, k);
                    self.op_src("lda", &s);
                    self.i("sta", Mode::Dp(0x1c + 2 * k as u8));
                }
                let code = match op {
                    BinOp::Mul => 0,
                    BinOp::DivU => 1,
                    BinOp::DivS => 2,
                    BinOp::RemU => 3,
                    _ => 4,
                };
                let irq = self.mi.interrupt_funcs.contains(&self.f.name);
                self.helpers.insert(Helper::A32(code, irq));
                if code != 0 {
                    self.helpers.insert(Helper::A32(1, irq));
                }
                let name = self.mi.helper_name(Helper::A32(code, irq));
                self.i("jsl", Mode::Label(name));
                self.forget();
                self.sta_reg(dst, 0);
                self.stx_reg(dst, 1);
                self.acc = None;
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
                if from == IrTy::I8 && !self.is_clean8(src) {
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
                self.begin8();
                self.mem_op("stz", &place, 0);
                self.end8();
                return;
            }
            if src_in_a || self.acc == Self::val_of(src, 0) && self.acc.is_some() {
                self.begin8();
            } else {
                // Load the byte in 8-bit mode too, so consecutive byte
                // stores share one `sep`.
                self.begin8();
                let s = self.src(src, 0);
                match s {
                    Src::Imm(Expr::Num(n)) => self.i("lda", Mode::Imm(Expr::Num(n & 0xff))),
                    Src::X => self.i("txa", Mode::Implied),
                    Src::Y => self.i("tya", Mode::Implied),
                    other => self.op_src("lda", &other),
                }
                self.acc = None;
            }
            self.mem_op("sta", &place, 0);
            self.end8();
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
                if self.home(*p) != self.home(dst) || self.home(dst) == Home::None {
                    self.lda(&Operand::Reg(*p), 1);
                    self.sta_reg(dst, 1);
                }
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
            self.begin8();
            if copy {
                self.i("lda", Mode::DpIndLongY(SCRATCH_PTR2));
            }
            self.i("sta", Mode::DpIndLongY(SCRATCH_PTR));
            self.end8();
        }
        self.forget();
    }

    // ------------------------------------------------------------------ calls

    fn call_helper(&mut self, h: Helper) {
        let h = if self.mi.interrupt_funcs.contains(&self.f.name) {
            match h {
                Helper::Mul16 => Helper::Mul16Soft,
                Helper::DivU16 => Helper::DivU16Soft,
                Helper::DivS16 => Helper::DivS16Soft,
                o => o,
            }
        } else {
            h
        };
        self.helpers.insert(h);
        if h == Helper::DivS16 {
            self.helpers.insert(Helper::DivU16);
        }
        if h == Helper::DivS16Soft {
            self.helpers.insert(Helper::DivU16Soft);
        }
        let name = self.mi.helper_name(h);
        self.i("jsl", Mode::Label(name));
        self.xv = None;
        self.yv = None;
        self.acc = None;
    }

    /// Writes a call's arguments into the callee's parameter homes: frame
    /// words first, then direct-page words in an order that never overwrites
    /// a word another argument still has to be read from, index registers
    /// last.
    fn pass_internal_args(&mut self, info: &crate::FuncInfo, args: &[Operand], sret: Option<&Addr>) {
        let callee_frame = |o: u32| Expr::Sym(info.frame_sym.clone(), o as i64);
        // (target home, source operand, words)
        let mut dp_moves: Vec<(u8, Operand, u32)> = Vec::new();
        let mut xy_moves: Vec<(Home, Operand)> = Vec::new();
        for (i, a) in args.iter().enumerate() {
            let Some(p) = info.params.get(i) else { break };
            match p {
                ParamKind::Scalar(t) => {
                    let words = self.width(*t);
                    match info.param_homes[i] {
                        Some(Home::Frame(o)) => {
                            for k in 0..words {
                                if self.src(a, k) == Src::Imm(Expr::Num(0)) {
                                    self.i("stz", Mode::Abs(callee_frame(o + 2 * k)));
                                } else {
                                    self.lda(a, k);
                                    self.i("sta", Mode::Abs(callee_frame(o + 2 * k)));
                                }
                            }
                        }
                        Some(Home::Dp(d)) => dp_moves.push((d, a.clone(), words)),
                        Some(h @ (Home::X | Home::Y)) => xy_moves.push((h, a.clone())),
                        _ => {}
                    }
                }
                ParamKind::Aggregate(n) => {
                    let po = info.param_offsets[i];
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
                        self.i("sta", Mode::Abs(callee_frame(po + k)));
                        k += 2;
                    }
                    self.forget();
                }
            }
        }
        if let Some(sa) = sret {
            self.addr_to_dp(sa, SCRATCH_PTR);
            match info.sret_home {
                Some(Home::Dp(d)) => {
                    for k in 0..2u8 {
                        self.i("lda", Mode::Dp(SCRATCH_PTR + 2 * k));
                        self.i("sta", Mode::Dp(d + 2 * k));
                    }
                }
                Some(Home::Frame(o)) => {
                    for k in 0..2u32 {
                        self.i("lda", Mode::Dp(SCRATCH_PTR + 2 * k as u8));
                        self.i("sta", Mode::Abs(callee_frame(o + 2 * k)));
                    }
                }
                _ => {}
            }
        }
        // Direct-page targets: parallel move.
        let reads = |o: &Operand, g: &Self| -> Vec<u8> {
            match o {
                Operand::Reg(r) => match g.home(*r) {
                    Home::Dp(d) => {
                        if g.width(g.f.ty(*r)) == 2 {
                            vec![d, d + 2]
                        } else {
                            vec![d]
                        }
                    }
                    _ => vec![],
                },
                _ => vec![],
            }
        };
        let mut pending = dp_moves;
        let mut stash: Vec<(u8, u32)> = Vec::new(); // pushed on the stack: (target, words)
        while !pending.is_empty() {
            let pos = pending.iter().position(|(d, _, words)| {
                let targets: Vec<u8> = (0..*words).map(|k| d + 2 * k as u8).collect();
                pending.iter().all(|(d2, src, _)| {
                    let _ = d2;
                    reads(src, self).iter().all(|r| !targets.contains(r))
                        || reads(src, self) == targets && *d2 == *d
                })
            });
            match pos {
                Some(p) => {
                    let (d, src, words) = pending.remove(p);
                    for k in 0..words {
                        if self.src(&src, k) == Src::Dp(d + 2 * k as u8) {
                            continue;
                        }
                        if self.src(&src, k) == Src::Imm(Expr::Num(0)) {
                            self.i("stz", Mode::Dp(d + 2 * k as u8));
                            continue;
                        }
                        self.lda(&src, k);
                        self.i("sta", Mode::Dp(d + 2 * k as u8));
                        self.acc = None;
                    }
                }
                None => {
                    // A cycle: park one source on the stack.
                    let (d, src, words) = pending.remove(0);
                    for k in (0..words).rev() {
                        self.lda(&src, k);
                        self.i("pha", Mode::Implied);
                    }
                    stash.push((d, words));
                }
            }
        }
        for (d, words) in stash.into_iter().rev() {
            for k in 0..words {
                self.i("pla", Mode::Implied);
                self.i("sta", Mode::Dp(d + 2 * k as u8));
            }
            self.acc = None;
        }
        // Index registers last (X and Y may swap).
        let xs = xy_moves.iter().find(|m| m.0 == Home::X).map(|m| m.1.clone());
        let ys = xy_moves.iter().find(|m| m.0 == Home::Y).map(|m| m.1.clone());
        let src_is = |o: &Option<Operand>, g: &Self, want: Src| o.as_ref().map_or(false, |o| g.src(o, 0) == want);
        if src_is(&xs, self, Src::Y) && src_is(&ys, self, Src::X) {
            self.i("txa", Mode::Implied);
            self.i("tyx", Mode::Implied);
            self.i("tay", Mode::Implied);
            self.acc = None;
        } else if src_is(&ys, self, Src::X) {
            // Y first reads X before X is overwritten.
            if let Some(y) = &ys {
                self.ldy(y);
            }
            if let Some(x) = &xs {
                self.ldx(x);
            }
        } else {
            if let Some(x) = &xs {
                self.ldx(x);
            }
            if let Some(y) = &ys {
                self.ldy(y);
            }
        }
        self.xv = None;
        self.yv = None;
    }

    fn gen_call(&mut self, dst: Option<VReg>, callee: &Callee, args: &[Operand], arg_tys: &[IrTy], sret: Option<&Addr>) {
        let internal = match callee {
            Callee::Direct(n) => self.mi.funcs.get(n).cloned(),
            Callee::Indirect(_) => None,
        };
        if let Some(info) = internal {
            // A call inside a recursive component: the callee may reuse any
            // member's static frame, this function's included, so every
            // member's frame is saved on the hardware stack around it.
            let callee_name = match callee {
                Callee::Direct(n) => n.clone(),
                _ => String::new(),
            };
            let scc = match (self.mi.scc_of.get(&self.f.name), self.mi.scc_of.get(&callee_name)) {
                (Some(a), Some(b)) if a == b => Some(*a),
                _ => None,
            };
            let saved: Vec<(String, u32)> = scc.map(|c| self.save_set(c)).unwrap_or_default();
            for (name, size) in &saved {
                let sym = self.mi.frame_symbol(name);
                for k in (0..size / 2).rev() {
                    self.i("lda", Mode::Abs(Expr::Sym(sym.clone(), 2 * k as i64)));
                    self.i("pha", Mode::Implied);
                }
            }
            self.acc = None;
            self.pass_internal_args(&info, args, sret);
            self.i("jsl", Mode::Label(info.body_label.clone()));
            if !saved.is_empty() {
                self.i("sta", Mode::Dp(0x18));
                self.i("stx", Mode::Dp(0x1a));
                for (name, size) in saved.iter().rev() {
                    let sym = self.mi.frame_symbol(name);
                    for k in 0..size / 2 {
                        self.i("pla", Mode::Implied);
                        self.i("sta", Mode::Abs(Expr::Sym(sym.clone(), 2 * k as i64)));
                    }
                }
                self.i("lda", Mode::Dp(0x18));
                self.i("ldx", Mode::Dp(0x1a));
            }
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
        // Code outside the module (or behind a pointer) may call back into
        // this function's recursive component: save its frames around the
        // call, as for direct recursion.
        let saved: Vec<(String, u32)> = match self.mi.scc_of.get(&self.f.name) {
            Some(&c) => self.save_set(c),
            None => Vec::new(),
        };
        for (name, size) in &saved {
            let sym = self.mi.frame_symbol(name);
            for k in (0..size / 2).rev() {
                self.i("lda", Mode::Abs(Expr::Sym(sym.clone(), 2 * k as i64)));
                self.i("pha", Mode::Implied);
            }
        }
        self.acc = None;
        self.gen_abi_call(dst, callee, args, arg_tys, sret);
        if !saved.is_empty() {
            self.i("lda", Mode::Dp(0));
            self.i("sta", Mode::Dp(0x18));
            self.i("lda", Mode::Dp(2));
            self.i("sta", Mode::Dp(0x1a));
            self.i("lda", Mode::Dp(4));
            self.i("sta", Mode::Dp(0x1c));
            for (name, size) in saved.iter().rev() {
                let sym = self.mi.frame_symbol(name);
                for k in 0..size / 2 {
                    self.i("pla", Mode::Implied);
                    self.i("sta", Mode::Abs(Expr::Sym(sym.clone(), 2 * k as i64)));
                }
            }
            // The result was already stored by gen_abi_call; a result homed
            // in the frame was just overwritten by the restore, so store it
            // again from the saved registers.
            self.forget();
            if let Some(d) = dst {
                let t = self.f.ty(d);
                self.i("lda", Mode::Dp(0x18));
                self.acc = None;
                self.sta_reg(d, 0);
                if self.width(t) == 2 {
                    self.i("lda", Mode::Dp(if t == IrTy::I32 { 0x1c } else { 0x1a }));
                    self.acc = None;
                    self.sta_reg(d, 1);
                    self.acc = None;
                    self.lda(&Operand::Reg(d), 0);
                }
            }
        }
    }

    /// Frames saved around a call that may re-enter component `c`. The
    /// calling function's own address-taken slots are left out: the callee
    /// may legitimately write them through a pointer (an out-parameter, a
    /// struct result), and restoring would undo that. (A re-entrant
    /// activation writing the same slot is the case static frames cannot
    /// cover; it is documented in PLAN.md.)
    fn save_set(&self, c: usize) -> Vec<(String, u32)> {
        let limit = self
            .f
            .slots
            .iter()
            .enumerate()
            .filter(|(i, _)| !self.f.param_slots.contains(&Some(SlotId(*i as u32))))
            .map(|(i, _)| self.al.slot_offsets[i])
            .min();
        self.mi.scc_members[c]
            .iter()
            .map(|(n, size)| {
                if *n == self.f.name {
                    (n.clone(), limit.map_or(*size, |l| l.min(*size) & !1))
                } else {
                    (n.clone(), *size)
                }
            })
            .collect()
    }

    fn gen_abi_call(&mut self, dst: Option<VReg>, callee: &Callee, args: &[Operand], arg_tys: &[IrTy], sret: Option<&Addr>) {
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
                        self.begin8();
                        self.i("lda", Mode::DpIndLongY(SCRATCH_PTR2));
                        self.i("sta", Mode::Sr((k + 1) as u8));
                        self.end8();
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
                    self.begin8();
                    self.i("pha", Mode::Implied);
                    self.end8();
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
                // 816-tcc returns a 32-bit integer's high word in tcc__r1.
                self.i("lda", Mode::Dp(if k == 1 && t == IrTy::I32 { 4 } else { 2 * k as u8 }));
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
                    }
                    // Both words are read before either result register is
                    // written (the value may live in $00-$07 itself).
                    self.lda(v, 0);
                    if self.width(t) == 2 && exported {
                        // 816-tcc: a pointer's bank in tcc__r0h ($02), a
                        // 32-bit integer's high word in tcc__r1 ($04).
                        self.i("sta", Mode::Dp(0));
                        self.i("stx", Mode::Dp(if t == IrTy::I32 { 4 } else { 2 }));
                    }
                    if t == IrTy::I8 {
                        // Callers through the 816-tcc ABI read the low byte;
                        // internal callers only the low byte too.
                    }
                    if exported && self.width(t) == 1 {
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
        // 8-bit operands whose high bytes are known zero compare in 16 bits.
        let eight = ty == IrTy::I8 && !(self.is_clean8(&a) && (b.imm().is_some() || self.is_clean8(&b)) && !cc.is_signed());
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
                        self.begin8();
                        self.i("sec", Mode::Implied);
                        self.i("sbc", Mode::Dp(SCRATCH_WORD));
                        let skip = self.fresh();
                        self.i("bvc", Mode::Label(skip.clone()));
                        self.i8("eor", Mode::Imm(Expr::Num(0x80)));
                        self.label(skip);
                        self.end8();
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
            self.begin8();
            if cc.is_signed() {
                self.signed_cmp8(&b);
            } else {
                match self.src(&b, 0) {
                    Src::Imm(Expr::Num(n)) => self.i8("cmp", Mode::Imm(Expr::Num(n & 0xff))),
                    s => self.op_src("cmp", &s),
                }
            }
            self.end8();
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
        // Unsigned and equality; an index-register operand compares with
        // cpx/cpy.
        if let (Src::X | Src::Y, false) = (self.src(&a, 0), self.fwd(&b)) {
            let bs = self.src(&b, 0);
            if matches!(bs, Src::Imm(_) | Src::Dp(_) | Src::Abs(_)) {
                let m = if self.src(&a, 0) == Src::X { "cpx" } else { "cpy" };
                if let (Some(k), Cond::GtU | Cond::LeU) = (bi, cc) {
                    if k < 0xffff {
                        self.i(m, Mode::Imm(w(k + 1)));
                        self.emit_cc_branch(if cc == Cond::GtU { Cond::GeU } else { Cond::LtU }, &lt, &lf);
                        return;
                    }
                }
                self.op_src(m, &bs);
                self.emit_cc_branch(cc, &lt, &lf);
                return;
            }
        }
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
                self.ensure16();
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

/// Block order: greedy traces so a block's preferred successor follows it
/// (the taken side of a two-way branch is the one not placed yet, so loop
/// latches fall out of the loop and `if` bodies fall through).
pub fn layout(f: &Func, reach: &[bool]) -> Vec<usize> {
    let n = f.blocks.len();
    let mut placed = vec![false; n];
    let mut order = Vec::new();
    let preds = f.preds();
    for start in 0..n {
        if placed[start] || !reach[start] {
            continue;
        }
        let mut b = start;
        loop {
            placed[b] = true;
            order.push(b);
            let next = match &f.blocks[b].term {
                Term::Jmp(t) => Some(*t),
                Term::Br { t, f: fb, .. } | Term::BrCmp { t, f: fb, .. } => {
                    // Prefer the successor with no other unplaced
                    // predecessor, then the true side.
                    let cands = [*t, *fb];
                    cands
                        .iter()
                        .copied()
                        .filter(|c| !placed[c.0 as usize])
                        .min_by_key(|c| preds[c.0 as usize].iter().filter(|p| !placed[p.0 as usize]).count())
                }
                _ => None,
            };
            // A join block waits until its earlier predecessors (the other
            // arm of an if) are placed, so both arms reach it with short
            // branches.
            let next = next.filter(|c| preds[c.0 as usize].iter().all(|p| placed[p.0 as usize] || p.0 >= c.0 || p.0 as usize == b));
            match next {
                Some(nb) if !placed[nb.0 as usize] && reach[nb.0 as usize] => b = nb.0 as usize,
                _ => break,
            }
        }
    }
    order
}
