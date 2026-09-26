//! The IR: a CFG of basic blocks of three-address instructions over typed
//! virtual registers. Memory accesses carry a structured address (base +
//! constant offset + scaled index) so the backend can choose 65816
//! addressing modes; pointers are 24-bit far addresses stored in four bytes
//! whose arithmetic touches only the low sixteen bits.

use std::fmt::{self, Write as _};

#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
pub enum IrTy {
    I8,
    I16,
    I32,
    /// A far pointer: bank:offset (24 bits, four bytes of storage).
    Ptr,
}

impl IrTy {
    pub fn size(self) -> u32 {
        match self {
            IrTy::I8 => 1,
            IrTy::I16 => 2,
            IrTy::I32 | IrTy::Ptr => 4,
        }
    }

    pub fn bits(self) -> u32 {
        match self {
            IrTy::I8 => 8,
            IrTy::I16 => 16,
            IrTy::I32 => 32,
            IrTy::Ptr => 24,
        }
    }

    pub fn mask(self) -> u64 {
        (1u64 << self.bits()) - 1
    }

    /// Sign-extends a value of this width to i64.
    pub fn sext(self, v: i64) -> i64 {
        let b = self.bits();
        let m = self.mask();
        let u = (v as u64) & m;
        if u >> (b - 1) != 0 {
            (u | !m) as i64
        } else {
            u as i64
        }
    }

    pub fn zext(self, v: i64) -> i64 {
        ((v as u64) & self.mask()) as i64
    }
}

impl fmt::Display for IrTy {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            IrTy::I8 => "i8",
            IrTy::I16 => "i16",
            IrTy::I32 => "i32",
            IrTy::Ptr => "ptr",
        })
    }
}

#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub struct VReg(pub u32);

#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub struct SlotId(pub u32);

#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub struct BlockId(pub u32);

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum Operand {
    Reg(VReg),
    Imm(i64),
    /// The address of a global symbol plus a byte offset (type ptr).
    Global(String, i64),
    /// The address of a frame slot plus a byte offset (type ptr).
    Slot(SlotId, i64),
}

impl Operand {
    pub fn reg(&self) -> Option<VReg> {
        match self {
            Operand::Reg(r) => Some(*r),
            _ => None,
        }
    }

    pub fn imm(&self) -> Option<i64> {
        match self {
            Operand::Imm(v) => Some(*v),
            _ => None,
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum Base {
    Global(String),
    Slot(SlotId),
    /// A pointer in a register.
    Reg(VReg),
    /// An absolute 24-bit address (hardware registers, casts of constants).
    Abs(u32),
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub struct Addr {
    pub base: Base,
    pub offset: i64,
    /// A register index multiplied by a constant scale (bytes).
    pub index: Option<(VReg, u32)>,
}

impl Addr {
    pub fn global(name: &str, offset: i64) -> Addr {
        Addr { base: Base::Global(name.to_string()), offset, index: None }
    }

    pub fn regs(&self) -> Vec<VReg> {
        let mut v = Vec::new();
        if let Base::Reg(r) = self.base {
            v.push(r);
        }
        if let Some((r, _)) = self.index {
            v.push(r);
        }
        v
    }
}

#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
pub enum BinOp {
    Add,
    Sub,
    Mul,
    DivS,
    DivU,
    RemS,
    RemU,
    And,
    Or,
    Xor,
    Shl,
    ShrS,
    ShrU,
}

impl BinOp {
    pub fn name(self) -> &'static str {
        match self {
            BinOp::Add => "add",
            BinOp::Sub => "sub",
            BinOp::Mul => "mul",
            BinOp::DivS => "divs",
            BinOp::DivU => "divu",
            BinOp::RemS => "rems",
            BinOp::RemU => "remu",
            BinOp::And => "and",
            BinOp::Or => "or",
            BinOp::Xor => "xor",
            BinOp::Shl => "shl",
            BinOp::ShrS => "shrs",
            BinOp::ShrU => "shru",
        }
    }

    pub fn commutative(self) -> bool {
        matches!(self, BinOp::Add | BinOp::Mul | BinOp::And | BinOp::Or | BinOp::Xor)
    }
}

#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
pub enum UnOp {
    Neg,
    Not,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
pub enum Cond {
    Eq,
    Ne,
    LtS,
    LeS,
    GtS,
    GeS,
    LtU,
    LeU,
    GtU,
    GeU,
}

impl Cond {
    pub fn name(self) -> &'static str {
        match self {
            Cond::Eq => "eq",
            Cond::Ne => "ne",
            Cond::LtS => "lts",
            Cond::LeS => "les",
            Cond::GtS => "gts",
            Cond::GeS => "ges",
            Cond::LtU => "ltu",
            Cond::LeU => "leu",
            Cond::GtU => "gtu",
            Cond::GeU => "geu",
        }
    }

    pub fn negate(self) -> Cond {
        match self {
            Cond::Eq => Cond::Ne,
            Cond::Ne => Cond::Eq,
            Cond::LtS => Cond::GeS,
            Cond::LeS => Cond::GtS,
            Cond::GtS => Cond::LeS,
            Cond::GeS => Cond::LtS,
            Cond::LtU => Cond::GeU,
            Cond::LeU => Cond::GtU,
            Cond::GtU => Cond::LeU,
            Cond::GeU => Cond::LtU,
        }
    }

    /// The condition with operands swapped.
    pub fn swap(self) -> Cond {
        match self {
            Cond::Eq => Cond::Eq,
            Cond::Ne => Cond::Ne,
            Cond::LtS => Cond::GtS,
            Cond::LeS => Cond::GeS,
            Cond::GtS => Cond::LtS,
            Cond::GeS => Cond::LeS,
            Cond::LtU => Cond::GtU,
            Cond::LeU => Cond::GeU,
            Cond::GtU => Cond::LtU,
            Cond::GeU => Cond::LeU,
        }
    }

    pub fn is_signed(self) -> bool {
        matches!(self, Cond::LtS | Cond::LeS | Cond::GtS | Cond::GeS)
    }

    /// Evaluates on values already reduced to the operand width `ty`.
    pub fn eval(self, ty: IrTy, a: i64, b: i64) -> bool {
        let (sa, sb) = (ty.sext(a), ty.sext(b));
        let (ua, ub) = (ty.zext(a) as u64, ty.zext(b) as u64);
        match self {
            Cond::Eq => ua == ub,
            Cond::Ne => ua != ub,
            Cond::LtS => sa < sb,
            Cond::LeS => sa <= sb,
            Cond::GtS => sa > sb,
            Cond::GeS => sa >= sb,
            Cond::LtU => ua < ub,
            Cond::LeU => ua <= ub,
            Cond::GtU => ua > ub,
            Cond::GeU => ua >= ub,
        }
    }
}

#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
pub enum ConvKind {
    Sext,
    Zext,
    Trunc,
}

#[derive(Clone, Debug, PartialEq)]
pub enum Callee {
    Direct(String),
    Indirect(Operand),
}

#[derive(Clone, Debug, PartialEq)]
pub enum Inst {
    Mov { dst: VReg, src: Operand },
    Bin { op: BinOp, dst: VReg, a: Operand, b: Operand },
    Un { op: UnOp, dst: VReg, a: Operand },
    /// dst (i16) = a cc b ? 1 : 0; operands compared at width `ty`.
    Cmp { cc: Cond, ty: IrTy, dst: VReg, a: Operand, b: Operand },
    /// Width conversion from `from` to dst's type.
    Conv { kind: ConvKind, dst: VReg, src: Operand, from: IrTy },
    Load { dst: VReg, addr: Addr, volatile: bool },
    Store { addr: Addr, src: Operand, ty: IrTy, volatile: bool },
    /// dst (ptr) = the address.
    Lea { dst: VReg, addr: Addr },
    /// Call; aggregate results are written through a hidden pointer passed as
    /// `sret`.
    Call { dst: Option<VReg>, callee: Callee, args: Vec<Operand>, arg_tys: Vec<IrTy>, arg_kinds: Vec<ParamKind>, sret: Option<Addr> },
    Memcpy { dst: Addr, src: Addr, size: u32 },
    Memset { dst: Addr, val: u8, size: u32 },
}

impl Inst {
    pub fn def(&self) -> Option<VReg> {
        match self {
            Inst::Mov { dst, .. }
            | Inst::Bin { dst, .. }
            | Inst::Un { dst, .. }
            | Inst::Cmp { dst, .. }
            | Inst::Conv { dst, .. }
            | Inst::Load { dst, .. }
            | Inst::Lea { dst, .. } => Some(*dst),
            Inst::Call { dst, .. } => *dst,
            _ => None,
        }
    }

    pub fn uses(&self) -> Vec<VReg> {
        let mut v = Vec::new();
        let op = |o: &Operand, v: &mut Vec<VReg>| {
            if let Operand::Reg(r) = o {
                v.push(*r);
            }
        };
        match self {
            Inst::Mov { src, .. } => op(src, &mut v),
            Inst::Bin { a, b, .. } | Inst::Cmp { a, b, .. } => {
                op(a, &mut v);
                op(b, &mut v);
            }
            Inst::Un { a, .. } => op(a, &mut v),
            Inst::Conv { src, .. } => op(src, &mut v),
            Inst::Load { addr, .. } | Inst::Lea { addr, .. } => v.extend(addr.regs()),
            Inst::Store { addr, src, .. } => {
                v.extend(addr.regs());
                op(src, &mut v);
            }
            Inst::Call { callee, args, sret, .. } => {
                if let Callee::Indirect(o) = callee {
                    op(o, &mut v);
                }
                for a in args {
                    op(a, &mut v);
                }
                if let Some(s) = sret {
                    v.extend(s.regs());
                }
            }
            Inst::Memcpy { dst, src, .. } => {
                v.extend(dst.regs());
                v.extend(src.regs());
            }
            Inst::Memset { dst, .. } => v.extend(dst.regs()),
        }
        v
    }

    /// Rewrites every register use.
    pub fn map_uses(&mut self, f: &mut dyn FnMut(VReg) -> Operand) {
        fn op(o: &mut Operand, f: &mut dyn FnMut(VReg) -> Operand) {
            if let Operand::Reg(r) = o {
                *o = f(*r);
            }
        }
        fn addr(a: &mut Addr, f: &mut dyn FnMut(VReg) -> Operand) {
            // Address registers can only be replaced by registers (or folded
            // by the caller); constants for a pointer base become Abs/Global.
            if let Base::Reg(r) = a.base {
                match f(r) {
                    Operand::Reg(n) => a.base = Base::Reg(n),
                    Operand::Global(g, o) => {
                        a.base = Base::Global(g);
                        a.offset += o;
                    }
                    Operand::Slot(s, o) => {
                        a.base = Base::Slot(s);
                        a.offset += o;
                    }
                    Operand::Imm(v) => {
                        a.base = Base::Abs((v as u32) & 0xff_ffff);
                    }
                }
            }
            if let Some((r, s)) = a.index {
                match f(r) {
                    Operand::Reg(n) => a.index = Some((n, s)),
                    Operand::Imm(v) => {
                        // Constant index: fold (16-bit address arithmetic).
                        a.offset += (v as i16 as i64) * s as i64;
                        a.index = None;
                    }
                    _ => {}
                }
            }
        }
        match self {
            Inst::Mov { src, .. } => op(src, f),
            Inst::Bin { a, b, .. } | Inst::Cmp { a, b, .. } => {
                op(a, f);
                op(b, f);
            }
            Inst::Un { a, .. } => op(a, f),
            Inst::Conv { src, .. } => op(src, f),
            Inst::Load { addr: a, .. } | Inst::Lea { addr: a, .. } => addr(a, f),
            Inst::Store { addr: a, src, .. } => {
                addr(a, f);
                op(src, f);
            }
            Inst::Call { callee, args, sret, .. } => {
                if let Callee::Indirect(o) = callee {
                    op(o, f);
                }
                for a in args {
                    op(a, f);
                }
                if let Some(s) = sret {
                    addr(s, f);
                }
            }
            Inst::Memcpy { dst, src, .. } => {
                addr(dst, f);
                addr(src, f);
            }
            Inst::Memset { dst, .. } => addr(dst, f),
        }
    }

    pub fn has_side_effects(&self) -> bool {
        match self {
            Inst::Store { .. } | Inst::Call { .. } | Inst::Memcpy { .. } | Inst::Memset { .. } => true,
            Inst::Load { volatile, .. } => *volatile,
            // Division by zero traps on no SNES path; treat as pure.
            _ => false,
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub enum Term {
    Jmp(BlockId),
    /// Branch on a nonzero operand.
    Br { cond: Operand, t: BlockId, f: BlockId },
    /// Fused compare and branch.
    BrCmp { cc: Cond, ty: IrTy, a: Operand, b: Operand, t: BlockId, f: BlockId },
    Switch { val: Operand, ty: IrTy, cases: Vec<(i64, BlockId)>, default: BlockId },
    Ret(Option<Operand>),
    Unreachable,
}

impl Term {
    pub fn succs(&self) -> Vec<BlockId> {
        match self {
            Term::Jmp(b) => vec![*b],
            Term::Br { t, f, .. } | Term::BrCmp { t, f, .. } => vec![*t, *f],
            Term::Switch { cases, default, .. } => {
                let mut v: Vec<BlockId> = cases.iter().map(|c| c.1).collect();
                v.push(*default);
                v
            }
            Term::Ret(_) | Term::Unreachable => vec![],
        }
    }

    pub fn succs_mut(&mut self) -> Vec<&mut BlockId> {
        match self {
            Term::Jmp(b) => vec![b],
            Term::Br { t, f, .. } | Term::BrCmp { t, f, .. } => vec![t, f],
            Term::Switch { cases, default, .. } => {
                let mut v: Vec<&mut BlockId> = cases.iter_mut().map(|c| &mut c.1).collect();
                v.push(default);
                v
            }
            Term::Ret(_) | Term::Unreachable => vec![],
        }
    }

    pub fn uses(&self) -> Vec<VReg> {
        let mut v = Vec::new();
        let mut op = |o: &Operand| {
            if let Operand::Reg(r) = o {
                v.push(*r);
            }
        };
        match self {
            Term::Br { cond, .. } => op(cond),
            Term::BrCmp { a, b, .. } => {
                op(a);
                op(b);
            }
            Term::Switch { val, .. } => op(val),
            Term::Ret(Some(o)) => op(o),
            _ => {}
        }
        v
    }

    pub fn map_uses(&mut self, f: &mut dyn FnMut(VReg) -> Operand) {
        let op = |o: &mut Operand, f: &mut dyn FnMut(VReg) -> Operand| {
            if let Operand::Reg(r) = o {
                *o = f(*r);
            }
        };
        match self {
            Term::Br { cond, .. } => op(cond, f),
            Term::BrCmp { a, b, .. } => {
                op(a, f);
                op(b, f);
            }
            Term::Switch { val, .. } => op(val, f),
            Term::Ret(Some(o)) => op(o, f),
            _ => {}
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct Block {
    pub insts: Vec<Inst>,
    pub term: Term,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Slot {
    pub size: u32,
    pub align: u32,
    pub name: String,
}

/// A function's parameter or result.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum ParamKind {
    Scalar(IrTy),
    /// A struct passed by value (its bytes), received into a slot.
    Aggregate(u32),
}

#[derive(Clone, Debug, PartialEq)]
pub struct Func {
    pub name: String,
    /// Visible to other objects (the 816-tcc ABI entry carries this name).
    pub exported: bool,
    pub params: Vec<ParamKind>,
    /// The register (scalars) or slot (aggregates) each parameter arrives in.
    pub param_regs: Vec<Option<VReg>>,
    pub param_slots: Vec<Option<SlotId>>,
    pub ret: Option<IrTy>,
    /// Aggregate result size (returned through a hidden pointer).
    pub sret: Option<u32>,
    /// The register holding the hidden result pointer.
    pub sret_reg: Option<VReg>,
    pub vregs: Vec<IrTy>,
    pub slots: Vec<Slot>,
    pub blocks: Vec<Block>,
    pub interrupt: bool,
    pub variadic: bool,
    /// Its address is taken (reachable through a function pointer).
    pub address_taken: bool,
    /// Registers that hold values of a signed C type, where arithmetic
    /// overflow is undefined (used by range analysis).
    pub signed: Vec<bool>,
}

impl Func {
    pub fn new_vreg(&mut self, ty: IrTy) -> VReg {
        self.vregs.push(ty);
        self.signed.push(false);
        VReg((self.vregs.len() - 1) as u32)
    }

    pub fn is_signed(&self, r: VReg) -> bool {
        self.signed.get(r.0 as usize).copied().unwrap_or(false)
    }

    pub fn ty(&self, r: VReg) -> IrTy {
        self.vregs[r.0 as usize]
    }

    /// Type of an operand (immediates take `hint`).
    pub fn op_ty(&self, o: &Operand, hint: IrTy) -> IrTy {
        match o {
            Operand::Reg(r) => self.ty(*r),
            Operand::Imm(_) => hint,
            Operand::Global(..) | Operand::Slot(..) => IrTy::Ptr,
        }
    }

    pub fn preds(&self) -> Vec<Vec<BlockId>> {
        let mut p = vec![Vec::new(); self.blocks.len()];
        for (i, b) in self.blocks.iter().enumerate() {
            for s in b.term.succs() {
                if !p[s.0 as usize].contains(&BlockId(i as u32)) {
                    p[s.0 as usize].push(BlockId(i as u32));
                }
            }
        }
        p
    }
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum Section {
    Bss,
    Data,
    Rodata,
}

#[derive(Clone, Debug, PartialEq)]
pub struct DataReloc {
    pub offset: u32,
    pub target: String,
    pub addend: i64,
    pub width: u8,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Global {
    pub name: String,
    pub size: u32,
    pub align: u32,
    pub section: Section,
    pub exported: bool,
    /// None: declared here, defined elsewhere.
    pub init: Option<(Vec<u8>, Vec<DataReloc>)>,
}

#[derive(Clone, Debug, Default, PartialEq)]
pub struct Module {
    pub globals: Vec<Global>,
    pub funcs: Vec<Func>,
    /// Functions referenced but not defined in the module, with their
    /// parameter kinds and result (called through the 816-tcc ABI).
    pub extern_funcs: Vec<(String, Vec<ParamKind>, Option<IrTy>, bool)>,
}

impl Module {
    pub fn func(&self, name: &str) -> Option<&Func> {
        self.funcs.iter().find(|f| f.name == name)
    }

    pub fn global(&self, name: &str) -> Option<&Global> {
        self.globals.iter().find(|g| g.name == name)
    }
}

// ---------------------------------------------------------------------- printing

impl fmt::Display for Operand {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Operand::Reg(r) => write!(f, "%{}", r.0),
            Operand::Imm(v) => write!(f, "{}", v),
            Operand::Global(g, 0) => write!(f, "@{}", g),
            Operand::Global(g, o) => write!(f, "@{}{:+}", g, o),
            Operand::Slot(s, 0) => write!(f, "$s{}", s.0),
            Operand::Slot(s, o) => write!(f, "$s{}{:+}", s.0, o),
        }
    }
}

impl fmt::Display for Addr {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str("[")?;
        match &self.base {
            Base::Global(g) => write!(f, "@{}", g)?,
            Base::Slot(s) => write!(f, "$s{}", s.0)?,
            Base::Reg(r) => write!(f, "%{}", r.0)?,
            Base::Abs(a) => write!(f, "${:06x}", a)?,
        }
        if self.offset != 0 {
            write!(f, "{:+}", self.offset)?;
        }
        if let Some((r, s)) = self.index {
            write!(f, " + %{}*{}", r.0, s)?;
        }
        f.write_str("]")
    }
}

pub fn print_inst(func: &Func, i: &Inst) -> String {
    let t = |r: &VReg| func.ty(*r);
    match i {
        Inst::Mov { dst, src } => format!("%{} = {} {}", dst.0, t(dst), src),
        Inst::Bin { op, dst, a, b } => format!("%{} = {} {} {}, {}", dst.0, op.name(), t(dst), a, b),
        Inst::Un { op, dst, a } => format!("%{} = {} {} {}", dst.0, if *op == UnOp::Neg { "neg" } else { "not" }, t(dst), a),
        Inst::Cmp { cc, ty, dst, a, b } => format!("%{} = cmp {} {} {}, {}", dst.0, cc.name(), ty, a, b),
        Inst::Conv { kind, dst, src, from } => format!(
            "%{} = {} {} {} to {}",
            dst.0,
            match kind {
                ConvKind::Sext => "sext",
                ConvKind::Zext => "zext",
                ConvKind::Trunc => "trunc",
            },
            from,
            src,
            t(dst)
        ),
        Inst::Load { dst, addr, volatile } => format!("%{} = load{} {} {}", dst.0, if *volatile { " volatile" } else { "" }, t(dst), addr),
        Inst::Store { addr, src, ty, volatile } => format!("store{} {} {}, {}", if *volatile { " volatile" } else { "" }, ty, src, addr),
        Inst::Lea { dst, addr } => format!("%{} = lea {}", dst.0, addr),
        Inst::Call { dst, callee, args, sret, .. } => {
            let mut s = String::new();
            if let Some(d) = dst {
                let _ = write!(s, "%{} = ", d.0);
            }
            s.push_str("call ");
            match callee {
                Callee::Direct(n) => s.push_str(n),
                Callee::Indirect(o) => {
                    let _ = write!(s, "*{}", o);
                }
            }
            s.push('(');
            for (k, a) in args.iter().enumerate() {
                if k > 0 {
                    s.push_str(", ");
                }
                let _ = write!(s, "{}", a);
            }
            s.push(')');
            if let Some(r) = sret {
                let _ = write!(s, " sret {}", r);
            }
            s
        }
        Inst::Memcpy { dst, src, size } => format!("memcpy {}, {}, {}", dst, src, size),
        Inst::Memset { dst, val, size } => format!("memset {}, {}, {}", dst, val, size),
    }
}

pub fn print_term(t: &Term) -> String {
    match t {
        Term::Jmp(b) => format!("jmp b{}", b.0),
        Term::Br { cond, t, f } => format!("br {}, b{}, b{}", cond, t.0, f.0),
        Term::BrCmp { cc, ty, a, b, t, f } => format!("br {} {} {}, {}, b{}, b{}", cc.name(), ty, a, b, t.0, f.0),
        Term::Switch { val, ty, cases, default } => {
            let mut s = format!("switch {} {} [", ty, val);
            for (k, (v, b)) in cases.iter().enumerate() {
                if k > 0 {
                    s.push_str(", ");
                }
                let _ = write!(s, "{}: b{}", v, b.0);
            }
            let _ = write!(s, "] default b{}", default.0);
            s
        }
        Term::Ret(None) => "ret".into(),
        Term::Ret(Some(o)) => format!("ret {}", o),
        Term::Unreachable => "unreachable".into(),
    }
}

pub fn print_func(f: &Func) -> String {
    let mut s = String::new();
    let _ = write!(s, "func {}{}(", if f.exported { "" } else { "static " }, f.name);
    for (i, p) in f.params.iter().enumerate() {
        if i > 0 {
            s.push_str(", ");
        }
        match (p, f.param_regs[i], f.param_slots[i]) {
            (ParamKind::Scalar(t), Some(r), _) => {
                let _ = write!(s, "{} %{}", t, r.0);
            }
            (ParamKind::Aggregate(n), _, Some(sl)) => {
                let _ = write!(s, "agg{} $s{}", n, sl.0);
            }
            (p, _, _) => {
                let _ = write!(s, "{:?}", p);
            }
        }
    }
    s.push(')');
    if let Some(r) = f.ret {
        let _ = write!(s, " -> {}", r);
    }
    if let Some(n) = f.sret {
        let _ = write!(s, " -> agg{}", n);
    }
    if f.interrupt {
        s.push_str(" interrupt");
    }
    s.push_str(" {\n");
    for (i, sl) in f.slots.iter().enumerate() {
        let _ = writeln!(s, "  $s{}: {} bytes align {} ; {}", i, sl.size, sl.align, sl.name);
    }
    for (i, b) in f.blocks.iter().enumerate() {
        let _ = writeln!(s, "b{}:", i);
        for inst in &b.insts {
            let _ = writeln!(s, "  {}", print_inst(f, inst));
        }
        let _ = writeln!(s, "  {}", print_term(&b.term));
    }
    s.push_str("}\n");
    s
}

pub fn print_module(m: &Module) -> String {
    let mut s = String::new();
    for g in &m.globals {
        let sec = match g.section {
            Section::Bss => "bss",
            Section::Data => "data",
            Section::Rodata => "rodata",
        };
        match &g.init {
            None => {
                let _ = writeln!(s, "extern {} @{} {} bytes", sec, g.name, g.size);
            }
            Some((bytes, relocs)) => {
                let _ = write!(s, "{}{} @{} {} bytes =", if g.exported { "" } else { "static " }, sec, g.name, g.size);
                if bytes.iter().all(|&b| b == 0) && relocs.is_empty() {
                    s.push_str(" zero");
                } else {
                    for b in bytes.iter().take(64) {
                        let _ = write!(s, " {:02x}", b);
                    }
                    if bytes.len() > 64 {
                        s.push_str(" ...");
                    }
                    for r in relocs {
                        let _ = write!(s, " [+{}: @{}{:+} w{}]", r.offset, r.target, r.addend, r.width);
                    }
                }
                s.push('\n');
            }
        }
    }
    for (name, _, _, _) in &m.extern_funcs {
        let _ = writeln!(s, "declare {}", name);
    }
    for f in &m.funcs {
        s.push('\n');
        s.push_str(&print_func(f));
    }
    s
}
