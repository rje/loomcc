//! The C17 abstract syntax tree (untyped; sema attaches types).

use loomcc_pp::Loc;

#[derive(Clone, Debug, PartialEq)]
pub struct TranslationUnit {
    pub items: Vec<ExternalDecl>,
}

#[derive(Clone, Debug, PartialEq)]
pub enum ExternalDecl {
    Function(FunctionDef),
    Decl(Declaration),
    StaticAssert(StaticAssert),
    Pragma(String, Loc),
}

#[derive(Clone, Debug, PartialEq)]
pub struct FunctionDef {
    pub specs: DeclSpecs,
    pub declarator: Declarator,
    /// K&R parameter declarations (old-style definitions).
    pub old_params: Vec<Declaration>,
    pub body: Block,
    pub loc: Loc,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Declaration {
    pub specs: DeclSpecs,
    pub declarators: Vec<InitDeclarator>,
    pub loc: Loc,
}

#[derive(Clone, Debug, PartialEq)]
pub struct InitDeclarator {
    pub declarator: Declarator,
    pub init: Option<Initializer>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Storage {
    Typedef,
    Extern,
    Static,
    Auto,
    Register,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Quals {
    pub is_const: bool,
    pub is_volatile: bool,
    pub is_restrict: bool,
    pub is_atomic: bool,
}

impl Quals {
    pub fn any(&self) -> bool {
        self.is_const || self.is_volatile || self.is_restrict || self.is_atomic
    }
}

/// Basic type keywords, counted (so `long long` and `unsigned` combine).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct BaseKeywords {
    pub void: u8,
    pub char: u8,
    pub short: u8,
    pub int: u8,
    pub long: u8,
    pub float: u8,
    pub double: u8,
    pub signed: u8,
    pub unsigned: u8,
    pub bool_: u8,
    pub complex: u8,
    pub int128: u8,
}

#[derive(Clone, Debug, PartialEq)]
pub enum TypeSpec {
    /// Only keyword specifiers (possibly none: implicit int).
    Keywords(BaseKeywords),
    Struct(StructSpec),
    Enum(EnumSpec),
    TypedefName(String),
    /// `typeof(expr)` / `typeof(type)` (GNU/C23).
    TypeofExpr(Box<Expr>),
    TypeofType(Box<TypeName>),
}

#[derive(Clone, Debug, PartialEq)]
pub struct DeclSpecs {
    pub storage: Option<Storage>,
    pub thread_local: bool,
    pub inline: bool,
    pub noreturn: bool,
    pub quals: Quals,
    pub ty: TypeSpec,
    pub align: Vec<AlignSpec>,
    pub attrs: Vec<Attribute>,
    pub loc: Loc,
}

#[derive(Clone, Debug, PartialEq)]
pub enum AlignSpec {
    Type(TypeName),
    Expr(Expr),
}

/// A GNU attribute or C23 `[[...]]` attribute, kept as name + raw tokens.
#[derive(Clone, Debug, PartialEq)]
pub struct Attribute {
    pub name: String,
    pub args: Vec<String>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum StructKind {
    Struct,
    Union,
}

#[derive(Clone, Debug, PartialEq)]
pub struct StructSpec {
    pub kind: StructKind,
    pub name: Option<String>,
    /// `None` for a reference (`struct S x;`), Some for a definition.
    pub members: Option<Vec<MemberDecl>>,
    pub attrs: Vec<Attribute>,
    pub loc: Loc,
}

#[derive(Clone, Debug, PartialEq)]
pub enum MemberDecl {
    Field {
        specs: DeclSpecs,
        /// Each declarator with an optional bit-field width. An empty list
        /// is an anonymous struct/union member.
        declarators: Vec<(Option<Declarator>, Option<Expr>)>,
        loc: Loc,
    },
    StaticAssert(StaticAssert),
}

#[derive(Clone, Debug, PartialEq)]
pub struct EnumSpec {
    pub name: Option<String>,
    pub variants: Option<Vec<Enumerator>>,
    pub loc: Loc,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Enumerator {
    pub name: String,
    pub value: Option<Expr>,
    pub loc: Loc,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Declarator {
    pub kind: DeclaratorKind,
    pub loc: Loc,
}

#[derive(Clone, Debug, PartialEq)]
pub enum DeclaratorKind {
    /// The name (None in an abstract declarator).
    Ident(Option<String>),
    Pointer(Quals, Box<Declarator>),
    Array {
        inner: Box<Declarator>,
        size: Option<Box<Expr>>,
        quals: Quals,
        is_static: bool,
        /// `[*]`
        vla_star: bool,
    },
    Function {
        inner: Box<Declarator>,
        params: Vec<ParamDecl>,
        variadic: bool,
        /// `f()` (no prototype) as opposed to `f(void)`.
        unprototyped: bool,
        /// K&R identifier list.
        old_names: Vec<String>,
    },
}

impl Declarator {
    pub fn name(&self) -> Option<&str> {
        match &self.kind {
            DeclaratorKind::Ident(n) => n.as_deref(),
            DeclaratorKind::Pointer(_, d) => d.name(),
            DeclaratorKind::Array { inner, .. } | DeclaratorKind::Function { inner, .. } => inner.name(),
        }
    }

    /// The location of the declared name.
    pub fn name_loc(&self) -> Loc {
        match &self.kind {
            DeclaratorKind::Ident(_) => self.loc,
            DeclaratorKind::Pointer(_, d) => d.name_loc(),
            DeclaratorKind::Array { inner, .. } | DeclaratorKind::Function { inner, .. } => inner.name_loc(),
        }
    }

    /// True when the outermost derivation applied to the name is a function
    /// (the declarator declares a function).
    pub fn is_function(&self) -> bool {
        match &self.kind {
            DeclaratorKind::Ident(_) => false,
            DeclaratorKind::Pointer(_, d) => {
                if matches!(d.kind, DeclaratorKind::Ident(_)) {
                    false
                } else {
                    d.is_function()
                }
            }
            DeclaratorKind::Array { inner, .. } => {
                if matches!(inner.kind, DeclaratorKind::Ident(_)) {
                    false
                } else {
                    inner.is_function()
                }
            }
            DeclaratorKind::Function { inner, .. } => {
                matches!(inner.kind, DeclaratorKind::Ident(_)) || inner.is_function()
            }
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct ParamDecl {
    pub specs: DeclSpecs,
    pub declarator: Declarator,
    pub loc: Loc,
}

#[derive(Clone, Debug, PartialEq)]
pub struct TypeName {
    pub specs: DeclSpecs,
    pub declarator: Declarator,
    pub loc: Loc,
}

#[derive(Clone, Debug, PartialEq)]
pub struct StaticAssert {
    pub cond: Expr,
    pub message: Option<String>,
    pub loc: Loc,
}

#[derive(Clone, Debug, PartialEq)]
pub enum Initializer {
    Expr(Expr),
    List(Vec<InitItem>, Loc),
}

#[derive(Clone, Debug, PartialEq)]
pub struct InitItem {
    pub designators: Vec<Designator>,
    pub init: Initializer,
}

#[derive(Clone, Debug, PartialEq)]
pub enum Designator {
    Index(Expr),
    /// GNU `[a ... b]`.
    Range(Expr, Expr),
    Field(String, Loc),
}

#[derive(Clone, Debug, PartialEq)]
pub struct Block {
    pub items: Vec<BlockItem>,
    pub loc: Loc,
    pub end: Loc,
}

#[derive(Clone, Debug, PartialEq)]
pub enum BlockItem {
    Decl(Declaration),
    StaticAssert(StaticAssert),
    Stmt(Stmt),
}

#[derive(Clone, Debug, PartialEq)]
pub struct Stmt {
    pub kind: StmtKind,
    pub loc: Loc,
}

#[derive(Clone, Debug, PartialEq)]
pub enum ForInit {
    Decl(Declaration),
    Expr(Expr),
}

#[derive(Clone, Debug, PartialEq)]
pub enum StmtKind {
    Compound(Block),
    Expr(Option<Expr>),
    If(Expr, Box<Stmt>, Option<Box<Stmt>>),
    While(Expr, Box<Stmt>),
    DoWhile(Box<Stmt>, Expr),
    For(Option<ForInit>, Option<Expr>, Option<Expr>, Box<Stmt>),
    Switch(Expr, Box<Stmt>),
    Case(Expr, Option<Expr>, Box<Stmt>),
    Default(Box<Stmt>),
    Labeled(String, Box<Stmt>),
    Goto(String),
    /// GNU computed goto `goto *e;`.
    GotoExpr(Expr),
    Continue,
    Break,
    Return(Option<Expr>),
    /// `asm("...")` statement, kept as text.
    Asm(String),
    Pragma(String),
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UnaryOp {
    Plus,
    Neg,
    Not,
    BitNot,
    Deref,
    AddrOf,
    PreInc,
    PreDec,
    PostInc,
    PostDec,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum BinaryOp {
    Mul,
    Div,
    Rem,
    Add,
    Sub,
    Shl,
    Shr,
    Lt,
    Gt,
    Le,
    Ge,
    Eq,
    Ne,
    BitAnd,
    BitXor,
    BitOr,
    LogAnd,
    LogOr,
}

impl BinaryOp {
    pub fn as_str(self) -> &'static str {
        use BinaryOp::*;
        match self {
            Mul => "*",
            Div => "/",
            Rem => "%",
            Add => "+",
            Sub => "-",
            Shl => "<<",
            Shr => ">>",
            Lt => "<",
            Gt => ">",
            Le => "<=",
            Ge => ">=",
            Eq => "==",
            Ne => "!=",
            BitAnd => "&",
            BitXor => "^",
            BitOr => "|",
            LogAnd => "&&",
            LogOr => "||",
        }
    }

    /// Binding strength (higher binds tighter).
    pub fn precedence(self) -> u8 {
        use BinaryOp::*;
        match self {
            Mul | Div | Rem => 10,
            Add | Sub => 9,
            Shl | Shr => 8,
            Lt | Gt | Le | Ge => 7,
            Eq | Ne => 6,
            BitAnd => 5,
            BitXor => 4,
            BitOr => 3,
            LogAnd => 2,
            LogOr => 1,
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct Expr {
    pub kind: ExprKind,
    pub loc: Loc,
}

#[derive(Clone, Debug, PartialEq)]
pub enum ExprKind {
    Ident(String),
    /// Integer or floating constant (pp-number spelling).
    Number(String),
    /// Character constant spelling including quotes and prefix.
    Char(String),
    /// Adjacent string literal spellings (concatenated by sema).
    Str(Vec<String>),
    Unary(UnaryOp, Box<Expr>),
    Binary(BinaryOp, Box<Expr>, Box<Expr>),
    /// `a = b` (op None) or `a op= b`.
    Assign(Option<BinaryOp>, Box<Expr>, Box<Expr>),
    Cond(Box<Expr>, Box<Expr>, Box<Expr>),
    Comma(Box<Expr>, Box<Expr>),
    Call(Box<Expr>, Vec<Expr>),
    Index(Box<Expr>, Box<Expr>),
    Member(Box<Expr>, String, bool),
    Cast(Box<TypeName>, Box<Expr>),
    SizeofExpr(Box<Expr>),
    SizeofType(Box<TypeName>),
    AlignofType(Box<TypeName>),
    AlignofExpr(Box<Expr>),
    CompoundLiteral(Box<TypeName>, Vec<InitItem>),
    Generic(Box<Expr>, Vec<(Option<TypeName>, Expr)>),
    /// GNU statement expression `({ ... })`.
    StmtExpr(Block),
    /// `__builtin_offsetof(type, member.designator[...])`.
    Offsetof(Box<TypeName>, Vec<Designator>),
    /// `__builtin_va_arg(ap, type)`.
    VaArg(Box<Expr>, Box<TypeName>),
}
