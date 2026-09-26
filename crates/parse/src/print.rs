//! Prints an AST back to C. `print(parse(print(ast))) == print(ast)` is the
//! parser's round-trip test.

use crate::ast::*;

pub fn print_unit(tu: &TranslationUnit) -> String {
    let mut p = Printer { out: String::new(), indent: 0 };
    for item in &tu.items {
        p.external(item);
    }
    p.out
}

pub fn print_expr(e: &Expr) -> String {
    let mut p = Printer { out: String::new(), indent: 0 };
    p.expr(e, 0);
    p.out
}

struct Printer {
    out: String,
    indent: usize,
}

impl Printer {
    fn w(&mut self, s: &str) {
        self.out.push_str(s);
    }

    fn nl(&mut self) {
        self.out.push('\n');
        for _ in 0..self.indent {
            self.out.push_str("    ");
        }
    }

    fn external(&mut self, item: &ExternalDecl) {
        match item {
            ExternalDecl::Function(f) => {
                self.specs(&f.specs);
                self.w(" ");
                self.declarator(&f.declarator);
                for d in &f.old_params {
                    self.nl();
                    self.declaration(d);
                }
                self.nl();
                self.block(&f.body);
                self.w("\n");
            }
            ExternalDecl::Decl(d) => {
                self.declaration(d);
                self.w("\n");
            }
            ExternalDecl::StaticAssert(s) => {
                self.static_assert(s);
                self.w("\n");
            }
            ExternalDecl::Pragma(p, _) => {
                self.w("#pragma ");
                self.w(p);
                self.w("\n");
            }
        }
    }

    fn static_assert(&mut self, s: &StaticAssert) {
        self.w("_Static_assert(");
        self.expr(&s.cond, 2);
        if let Some(m) = &s.message {
            self.w(", ");
            self.w(m);
        }
        self.w(");");
    }

    fn declaration(&mut self, d: &Declaration) {
        self.specs(&d.specs);
        for (i, id) in d.declarators.iter().enumerate() {
            self.w(if i == 0 { " " } else { ", " });
            self.declarator(&id.declarator);
            if let Some(init) = &id.init {
                self.w(" = ");
                self.initializer(init);
            }
        }
        self.w(";");
    }

    fn quals(&mut self, q: &Quals) {
        if q.is_const {
            self.w("const ");
        }
        if q.is_volatile {
            self.w("volatile ");
        }
        if q.is_restrict {
            self.w("restrict ");
        }
        if q.is_atomic {
            self.w("_Atomic ");
        }
    }

    fn specs(&mut self, s: &DeclSpecs) {
        match s.storage {
            Some(Storage::Typedef) => self.w("typedef "),
            Some(Storage::Extern) => self.w("extern "),
            Some(Storage::Static) => self.w("static "),
            Some(Storage::Auto) => self.w("auto "),
            Some(Storage::Register) => self.w("register "),
            None => {}
        }
        if s.thread_local {
            self.w("_Thread_local ");
        }
        if s.inline {
            self.w("inline ");
        }
        if s.noreturn {
            self.w("_Noreturn ");
        }
        for a in &s.align {
            self.w("_Alignas(");
            match a {
                AlignSpec::Type(t) => self.type_name(t),
                AlignSpec::Expr(e) => self.expr(e, 2),
            }
            self.w(") ");
        }
        self.quals(&s.quals);
        match &s.ty {
            TypeSpec::Keywords(k) => {
                let mut words = Vec::new();
                for _ in 0..k.signed {
                    words.push("signed");
                }
                for _ in 0..k.unsigned {
                    words.push("unsigned");
                }
                for _ in 0..k.short {
                    words.push("short");
                }
                for _ in 0..k.long {
                    words.push("long");
                }
                for _ in 0..k.void {
                    words.push("void");
                }
                for _ in 0..k.char {
                    words.push("char");
                }
                for _ in 0..k.int {
                    words.push("int");
                }
                for _ in 0..k.float {
                    words.push("float");
                }
                for _ in 0..k.double {
                    words.push("double");
                }
                for _ in 0..k.bool_ {
                    words.push("_Bool");
                }
                for _ in 0..k.complex {
                    words.push("_Complex");
                }
                for _ in 0..k.int128 {
                    words.push("__int128");
                }
                if words.is_empty() {
                    words.push("int");
                }
                self.w(&words.join(" "));
            }
            TypeSpec::Struct(st) => {
                self.w(if st.kind == StructKind::Struct { "struct" } else { "union" });
                if let Some(n) = &st.name {
                    self.w(" ");
                    self.w(n);
                }
                if let Some(ms) = &st.members {
                    self.w(" {");
                    self.indent += 1;
                    for m in ms {
                        self.nl();
                        match m {
                            MemberDecl::Field { specs, declarators, .. } => {
                                self.specs(specs);
                                for (i, (d, width)) in declarators.iter().enumerate() {
                                    self.w(if i == 0 { " " } else { ", " });
                                    if let Some(d) = d {
                                        self.declarator(d);
                                    }
                                    if let Some(wd) = width {
                                        self.w(" : ");
                                        self.expr(wd, 2);
                                    }
                                }
                                self.w(";");
                            }
                            MemberDecl::StaticAssert(s) => self.static_assert(s),
                        }
                    }
                    self.indent -= 1;
                    self.nl();
                    self.w("}");
                }
            }
            TypeSpec::Enum(en) => {
                self.w("enum");
                if let Some(n) = &en.name {
                    self.w(" ");
                    self.w(n);
                }
                if let Some(vs) = &en.variants {
                    self.w(" { ");
                    for (i, v) in vs.iter().enumerate() {
                        if i > 0 {
                            self.w(", ");
                        }
                        self.w(&v.name);
                        if let Some(e) = &v.value {
                            self.w(" = ");
                            self.expr(e, 2);
                        }
                    }
                    self.w(" }");
                }
            }
            TypeSpec::TypedefName(n) => self.w(n),
            TypeSpec::TypeofExpr(e) => {
                self.w("typeof(");
                self.expr(e, 0);
                self.w(")");
            }
            TypeSpec::TypeofType(t) => {
                self.w("typeof(");
                self.type_name(t);
                self.w(")");
            }
        }
    }

    fn type_name(&mut self, t: &TypeName) {
        self.specs(&t.specs);
        let mut inner = String::new();
        std::mem::swap(&mut inner, &mut self.out);
        self.declarator(&t.declarator);
        std::mem::swap(&mut inner, &mut self.out);
        if !inner.is_empty() {
            self.w(" ");
            self.w(&inner);
        }
    }

    fn declarator(&mut self, d: &Declarator) {
        match &d.kind {
            DeclaratorKind::Ident(Some(n)) => self.w(n),
            DeclaratorKind::Ident(None) => {}
            DeclaratorKind::Pointer(q, inner) => {
                self.w("*");
                if q.any() {
                    self.w(" ");
                    self.quals(q);
                }
                self.declarator(inner);
            }
            DeclaratorKind::Array { inner, size, quals, is_static, vla_star } => {
                self.suffix_inner(inner);
                self.w("[");
                if *is_static {
                    self.w("static ");
                }
                self.quals(quals);
                if *vla_star {
                    self.w("*");
                }
                if let Some(s) = size {
                    self.expr(s, 2);
                }
                self.w("]");
            }
            DeclaratorKind::Function { inner, params, variadic, unprototyped, old_names } => {
                self.suffix_inner(inner);
                self.w("(");
                if !old_names.is_empty() {
                    self.w(&old_names.join(", "));
                } else if params.is_empty() && !*variadic {
                    if !*unprototyped {
                        self.w("void");
                    }
                } else {
                    for (i, p) in params.iter().enumerate() {
                        if i > 0 {
                            self.w(", ");
                        }
                        self.specs(&p.specs);
                        let mut inner = String::new();
                        std::mem::swap(&mut inner, &mut self.out);
                        self.declarator(&p.declarator);
                        std::mem::swap(&mut inner, &mut self.out);
                        if !inner.is_empty() {
                            self.w(" ");
                            self.w(&inner);
                        }
                    }
                    if *variadic {
                        self.w(", ...");
                    }
                }
                self.w(")");
            }
        }
    }

    /// The declarator a suffix applies to: a pointer there needs parentheses.
    fn suffix_inner(&mut self, inner: &Declarator) {
        if matches!(inner.kind, DeclaratorKind::Pointer(..)) {
            self.w("(");
            self.declarator(inner);
            self.w(")");
        } else {
            self.declarator(inner);
        }
    }

    fn initializer(&mut self, i: &Initializer) {
        match i {
            Initializer::Expr(e) => self.expr(e, 2),
            Initializer::List(items, _) => self.init_items(items),
        }
    }

    fn init_items(&mut self, items: &[InitItem]) {
        self.w("{ ");
        for (k, it) in items.iter().enumerate() {
            if k > 0 {
                self.w(", ");
            }
            for d in &it.designators {
                match d {
                    Designator::Index(e) => {
                        self.w("[");
                        self.expr(e, 2);
                        self.w("]");
                    }
                    Designator::Range(a, b) => {
                        self.w("[");
                        self.expr(a, 2);
                        self.w(" ... ");
                        self.expr(b, 2);
                        self.w("]");
                    }
                    Designator::Field(n, _) => {
                        self.w(".");
                        self.w(n);
                    }
                }
            }
            if !it.designators.is_empty() {
                self.w(" = ");
            }
            self.initializer(&it.init);
        }
        self.w(" }");
    }

    fn block(&mut self, b: &Block) {
        self.w("{");
        self.indent += 1;
        for item in &b.items {
            self.nl();
            match item {
                BlockItem::Decl(d) => self.declaration(d),
                BlockItem::StaticAssert(s) => self.static_assert(s),
                BlockItem::Stmt(s) => self.stmt(s),
            }
        }
        self.indent -= 1;
        self.nl();
        self.w("}");
    }

    fn stmt(&mut self, s: &Stmt) {
        match &s.kind {
            StmtKind::Compound(b) => self.block(b),
            StmtKind::Expr(None) => self.w(";"),
            StmtKind::Expr(Some(e)) => {
                self.expr(e, 0);
                self.w(";");
            }
            StmtKind::If(c, a, b) => {
                self.w("if (");
                self.expr(c, 0);
                self.w(") ");
                self.braced(a);
                if let Some(b) = b {
                    self.w(" else ");
                    self.braced(b);
                }
            }
            StmtKind::While(c, body) => {
                self.w("while (");
                self.expr(c, 0);
                self.w(") ");
                self.braced(body);
            }
            StmtKind::DoWhile(body, c) => {
                self.w("do ");
                self.braced(body);
                self.w(" while (");
                self.expr(c, 0);
                self.w(");");
            }
            StmtKind::For(init, c, step, body) => {
                self.w("for (");
                match init {
                    Some(ForInit::Decl(d)) => self.declaration(d),
                    Some(ForInit::Expr(e)) => {
                        self.expr(e, 0);
                        self.w(";");
                    }
                    None => self.w(";"),
                }
                if let Some(c) = c {
                    self.w(" ");
                    self.expr(c, 0);
                }
                self.w(";");
                if let Some(s) = step {
                    self.w(" ");
                    self.expr(s, 0);
                }
                self.w(") ");
                self.braced(body);
            }
            StmtKind::Switch(c, body) => {
                self.w("switch (");
                self.expr(c, 0);
                self.w(") ");
                self.braced(body);
            }
            StmtKind::Case(v, hi, body) => {
                self.w("case ");
                self.expr(v, 2);
                if let Some(h) = hi {
                    self.w(" ... ");
                    self.expr(h, 2);
                }
                self.w(": ");
                self.stmt(body);
            }
            StmtKind::Default(body) => {
                self.w("default: ");
                self.stmt(body);
            }
            StmtKind::Labeled(l, body) => {
                self.w(l);
                self.w(": ");
                self.stmt(body);
            }
            StmtKind::Goto(l) => {
                self.w("goto ");
                self.w(l);
                self.w(";");
            }
            StmtKind::GotoExpr(e) => {
                self.w("goto *");
                self.expr(e, 14);
                self.w(";");
            }
            StmtKind::Continue => self.w("continue;"),
            StmtKind::Break => self.w("break;"),
            StmtKind::Return(e) => {
                self.w("return");
                if let Some(e) = e {
                    self.w(" ");
                    self.expr(e, 0);
                }
                self.w(";");
            }
            StmtKind::Asm(text) => {
                self.w("__asm__(");
                self.w(text);
                self.w(");");
            }
            StmtKind::Pragma(p) => {
                self.w("\n#pragma ");
                self.w(p);
                self.nl();
            }
        }
    }

    fn braced(&mut self, s: &Stmt) {
        if let StmtKind::Compound(b) = &s.kind {
            self.block(b);
        } else {
            self.w("{");
            self.indent += 1;
            self.nl();
            self.stmt(s);
            self.indent -= 1;
            self.nl();
            self.w("}");
        }
    }

    /// Prints with parentheses when the expression binds looser than `min`.
    /// Levels: 0 comma, 1 assignment, 2 conditional, 3.. binary (precedence
    /// + 2), 13 unary/cast, 14 postfix/primary.
    fn expr(&mut self, e: &Expr, min: u8) {
        let level = expr_level(e);
        let paren = level < min;
        if paren {
            self.w("(");
        }
        match &e.kind {
            ExprKind::Ident(n) => self.w(n),
            ExprKind::Number(n) | ExprKind::Char(n) => self.w(n),
            ExprKind::Str(parts) => self.w(&parts.join(" ")),
            ExprKind::Unary(op, a) => {
                let (pre, post) = match op {
                    UnaryOp::Plus => ("+", ""),
                    UnaryOp::Neg => ("-", ""),
                    UnaryOp::Not => ("!", ""),
                    UnaryOp::BitNot => ("~", ""),
                    UnaryOp::Deref => ("*", ""),
                    UnaryOp::AddrOf => ("&", ""),
                    UnaryOp::PreInc => ("++", ""),
                    UnaryOp::PreDec => ("--", ""),
                    UnaryOp::PostInc => ("", "++"),
                    UnaryOp::PostDec => ("", "--"),
                };
                if post.is_empty() {
                    self.w(pre);
                    // Avoid `- -x` gluing into `--x`.
                    let inner = print_expr(a);
                    if (pre == "-" && inner.starts_with('-')) || (pre == "+" && inner.starts_with('+')) || (pre == "&" && inner.starts_with('&')) {
                        self.w(" ");
                    }
                    self.expr(a, 13);
                } else {
                    self.expr(a, 14);
                    self.w(post);
                }
            }
            ExprKind::Binary(op, a, b) => {
                let p = op.precedence() + 2;
                self.expr(a, p);
                self.w(" ");
                self.w(op.as_str());
                self.w(" ");
                self.expr(b, p + 1);
            }
            ExprKind::Assign(op, a, b) => {
                self.expr(a, 13);
                self.w(" ");
                if let Some(op) = op {
                    self.w(op.as_str());
                }
                self.w("= ");
                self.expr(b, 1);
            }
            ExprKind::Cond(c, a, b) => {
                self.expr(c, 3);
                self.w(" ? ");
                self.expr(a, 0);
                self.w(" : ");
                self.expr(b, 2);
            }
            ExprKind::Comma(a, b) => {
                self.expr(a, 0);
                self.w(", ");
                self.expr(b, 1);
            }
            ExprKind::Call(f, args) => {
                self.expr(f, 14);
                self.w("(");
                for (i, a) in args.iter().enumerate() {
                    if i > 0 {
                        self.w(", ");
                    }
                    self.expr(a, 1);
                }
                self.w(")");
            }
            ExprKind::Index(a, i) => {
                self.expr(a, 14);
                self.w("[");
                self.expr(i, 0);
                self.w("]");
            }
            ExprKind::Member(a, n, arrow) => {
                self.expr(a, 14);
                self.w(if *arrow { "->" } else { "." });
                self.w(n);
            }
            ExprKind::Cast(t, a) => {
                self.w("(");
                self.type_name(t);
                self.w(")");
                self.expr(a, 13);
            }
            ExprKind::SizeofExpr(a) => {
                self.w("sizeof ");
                self.expr(a, 13);
            }
            ExprKind::SizeofType(t) => {
                self.w("sizeof(");
                self.type_name(t);
                self.w(")");
            }
            ExprKind::AlignofType(t) => {
                self.w("_Alignof(");
                self.type_name(t);
                self.w(")");
            }
            ExprKind::AlignofExpr(a) => {
                self.w("_Alignof ");
                self.expr(a, 13);
            }
            ExprKind::CompoundLiteral(t, items) => {
                self.w("(");
                self.type_name(t);
                self.w(")");
                self.init_items(items);
            }
            ExprKind::Generic(c, assocs) => {
                self.w("_Generic(");
                self.expr(c, 1);
                for (t, e) in assocs {
                    self.w(", ");
                    match t {
                        Some(t) => self.type_name(t),
                        None => self.w("default"),
                    }
                    self.w(": ");
                    self.expr(e, 1);
                }
                self.w(")");
            }
            ExprKind::StmtExpr(b) => {
                self.w("(");
                self.block(b);
                self.w(")");
            }
            ExprKind::Offsetof(t, ds) => {
                self.w("__builtin_offsetof(");
                self.type_name(t);
                self.w(", ");
                for (i, d) in ds.iter().enumerate() {
                    match d {
                        Designator::Field(n, _) => {
                            if i > 0 {
                                self.w(".");
                            }
                            self.w(n);
                        }
                        Designator::Index(e) => {
                            self.w("[");
                            self.expr(e, 0);
                            self.w("]");
                        }
                        Designator::Range(..) => {}
                    }
                }
                self.w(")");
            }
            ExprKind::VaArg(ap, t) => {
                self.w("__builtin_va_arg(");
                self.expr(ap, 1);
                self.w(", ");
                self.type_name(t);
                self.w(")");
            }
        }
        if paren {
            self.w(")");
        }
    }
}

fn expr_level(e: &Expr) -> u8 {
    match &e.kind {
        ExprKind::Comma(..) => 0,
        ExprKind::Assign(..) => 1,
        ExprKind::Cond(..) => 2,
        ExprKind::Binary(op, ..) => op.precedence() + 2,
        ExprKind::Unary(UnaryOp::PostInc | UnaryOp::PostDec, _) => 14,
        ExprKind::Unary(..) | ExprKind::Cast(..) | ExprKind::SizeofExpr(_) | ExprKind::AlignofExpr(_) => 13,
        _ => 14,
    }
}
