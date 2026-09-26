//! The typed tree sema produces: every expression carries its type, every
//! implicit conversion is an explicit `Cast`, names are resolved, and static
//! initialisers are evaluated to bytes and relocations.

use crate::types::Ty;
use loomcc_pp::Loc;

#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub struct GlobalId(pub u32);

#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub struct LocalId(pub u32);

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum Linkage {
    External,
    Internal,
}

/// Where a global's storage lives on the SNES.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum Section {
    /// Zero-initialised RAM (`.bss`, bank $7E).
    Bss,
    /// Initialised RAM (`globram.data`, bank $7F, copied from ROM by crt0).
    Data,
    /// Read-only data in ROM (`.rodata`).
    Rodata,
}

#[derive(Clone, Debug)]
pub struct Global {
    pub name: String,
    pub ty: Ty,
    pub linkage: Linkage,
    /// A function (`ty` is a function type).
    pub is_func: bool,
    /// Defined in this translation unit (object with storage, or function
    /// with a body).
    pub defined: bool,
    pub init: Option<StaticInit>,
    pub section: Section,
    pub loc: Loc,
    /// Was the address taken (functions: used other than as a call target).
    pub address_taken: bool,
    /// The body for a defined function.
    pub func: Option<Function>,
}

/// A static initialiser: bytes plus relocations (address constants).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct StaticInit {
    pub bytes: Vec<u8>,
    pub relocs: Vec<Reloc>,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Reloc {
    /// Byte offset in `bytes` where a four-byte far address goes.
    pub offset: u64,
    pub target: GlobalId,
    pub addend: i64,
    /// Bytes the address occupies (4 for a pointer; 2 when a pointer is
    /// converted to a 16-bit integer, e.g. `(u16)&x`).
    pub width: u8,
}

#[derive(Clone, Debug)]
pub struct Local {
    pub name: String,
    pub ty: Ty,
    pub is_param: bool,
    pub address_taken: bool,
    /// `static` local: the storage is a global.
    pub static_global: Option<GlobalId>,
    pub loc: Loc,
}

#[derive(Clone, Debug)]
pub struct Function {
    pub params: Vec<LocalId>,
    pub locals: Vec<Local>,
    pub body: Stmt,
    pub variadic: bool,
    /// `#pragma loomcc interrupt`, or passed to nmiSet.
    pub interrupt: bool,
}

#[derive(Clone, Debug)]
pub struct Stmt {
    pub kind: StmtKind,
    pub loc: Loc,
}

#[derive(Clone, Debug)]
pub enum StmtKind {
    Block(Vec<Stmt>),
    Expr(Expr),
    /// A local's declaration: its initialisation, if any, as statements.
    Decl(LocalId, Vec<Stmt>),
    If(Expr, Box<Stmt>, Option<Box<Stmt>>),
    While(Expr, Box<Stmt>),
    DoWhile(Box<Stmt>, Expr),
    For(Option<Box<Stmt>>, Option<Expr>, Option<Expr>, Box<Stmt>),
    /// Controlling expression (promoted), body, the case values (lo, hi,
    /// label index), whether it has a default.
    Switch(Expr, Box<Stmt>, Vec<(i64, i64, u32)>, Option<u32>),
    /// A case or default label inside a switch body (index into its list).
    CaseLabel(u32),
    Label(String),
    Goto(String),
    Break,
    Continue,
    Return(Option<Expr>),
    Empty,
}

#[derive(Clone, Debug)]
pub struct Expr {
    pub kind: ExprKind,
    pub ty: Ty,
    pub loc: Loc,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum BinOp {
    Add,
    Sub,
    Mul,
    Div,
    Rem,
    And,
    Or,
    Xor,
    Shl,
    Shr,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum CmpOp {
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum UnOp {
    Neg,
    BitNot,
    /// Logical not: operand scalar, result int 0/1.
    Not,
}

#[derive(Clone, Debug)]
pub enum ExprKind {
    IntConst(i64),
    FloatConst(f64),
    /// An lvalue naming a local.
    Local(LocalId),
    /// An lvalue naming a global object, or a function designator.
    Global(GlobalId),
    /// Arithmetic on operands already converted to `ty` (shifts: left operand
    /// promoted, right operand its own promoted type).
    Binary(BinOp, Box<Expr>, Box<Expr>),
    /// Comparison of operands converted to a common type; result int.
    Cmp(CmpOp, Box<Expr>, Box<Expr>),
    Unary(UnOp, Box<Expr>),
    LogAnd(Box<Expr>, Box<Expr>),
    LogOr(Box<Expr>, Box<Expr>),
    Cond(Box<Expr>, Box<Expr>, Box<Expr>),
    Comma(Box<Expr>, Box<Expr>),
    /// Pointer + integer index scaled by the pointee size (`scale` bytes).
    PtrAdd(Box<Expr>, Box<Expr>, u64),
    /// (a - b) / scale as ptrdiff_t.
    PtrDiff(Box<Expr>, Box<Expr>, u64),
    /// Conversion to `ty` (the operand's type is `inner.ty`).
    Cast(Box<Expr>),
    /// Lvalue: the object a pointer points to.
    Deref(Box<Expr>),
    /// Address of an lvalue (also array-to-pointer and function-to-pointer
    /// decay).
    AddrOf(Box<Expr>),
    /// Lvalue: a member of a struct/union lvalue at a byte offset.
    Member(Box<Expr>, u64),
    /// Lvalue: a bit-field member (object lvalue, byte offset of the unit,
    /// bit offset, width, unit size in bytes, signed).
    BitField(Box<Expr>, u64, u32, u32, u32, bool),
    /// `lhs = rhs` (rhs converted to the lhs type); value is the new lhs.
    Assign(Box<Expr>, Box<Expr>),
    /// `lhs op= rhs`: computed in `op_ty` then converted back.
    CompoundAssign(BinOp, Box<Expr>, Box<Expr>, Ty),
    /// `lhs += scale*rhs` / `lhs -= ...` on a pointer lvalue (`sub` false/true).
    PtrCompoundAssign(Box<Expr>, Box<Expr>, u64, bool),
    /// ++/-- on an lvalue: (lvalue, is_increment, is_prefix, step) where step
    /// is 1 for arithmetic types or the pointee size for pointers.
    IncDec(Box<Expr>, bool, bool, u64),
    Call(Box<Expr>, Vec<Expr>),
    /// A string literal (an array lvalue of a synthesized global).
    Str(GlobalId),
    /// A compound literal (lvalue): the local that holds it, with its
    /// initialisation statements.
    CompoundLiteral(LocalId, Vec<Stmt>),
    StmtExpr(Vec<Stmt>, Option<Box<Expr>>),
    /// Zero-fill of an lvalue's whole object (for initialisers).
    ZeroInit(Box<Expr>),
    /// `__builtin_va_arg` / `va_start` / `va_end` (unsupported in the
    /// backend; kept for the front end).
    VaArg(Box<Expr>),
    VaStart(Box<Expr>),
    VaEnd(Box<Expr>),
}

impl Expr {
    pub fn is_lvalue(&self) -> bool {
        matches!(
            self.kind,
            ExprKind::Local(_)
                | ExprKind::Global(_)
                | ExprKind::Deref(_)
                | ExprKind::Member(..)
                | ExprKind::BitField(..)
                | ExprKind::Str(_)
                | ExprKind::CompoundLiteral(..)
        )
    }
}

/// One checked translation unit.
pub struct Unit {
    pub types: crate::types::Types,
    pub globals: Vec<Global>,
    /// The unit's file name (statics are private to it).
    pub name: String,
    /// Struct/union/enum tags declared at file scope.
    pub file_tags: Vec<(String, Ty)>,
}

impl Unit {
    pub fn global(&self, id: GlobalId) -> &Global {
        &self.globals[id.0 as usize]
    }
}
