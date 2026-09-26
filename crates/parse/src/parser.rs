//! Recursive-descent C17 parser (with the common GNU extensions that system
//! headers use).

use crate::ast::*;
use loomcc_pp::{Diag, Loc, Punct, Token, TokenKind};
use std::collections::HashMap;

pub struct Parser<'a> {
    toks: &'a [Token],
    pos: usize,
    /// Scopes of ordinary identifiers: true = typedef name.
    scopes: Vec<HashMap<String, bool>>,
    pub diags: Vec<Diag>,
}

type PResult<T> = Result<T, ()>;

const KEYWORDS: &[&str] = &[
    "auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", "extern", "float",
    "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return", "short", "signed", "sizeof",
    "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while", "_Alignas",
    "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary", "_Noreturn", "_Static_assert",
    "_Thread_local", "__attribute__", "__attribute", "__asm__", "__asm", "asm", "__inline", "__inline__",
    "__restrict", "__restrict__", "__volatile__", "__volatile", "__const", "__const__", "__signed__", "__signed",
    "__extension__", "typeof", "__typeof__", "__typeof", "__alignof__", "__alignof", "alignof", "alignas",
    "static_assert", "__builtin_offsetof", "__builtin_va_arg", "__int128", "__int128_t", "__uint128_t",
];

pub fn is_keyword(s: &str) -> bool {
    KEYWORDS.contains(&s)
}

impl<'a> Parser<'a> {
    pub fn new(toks: &'a [Token]) -> Parser<'a> {
        let mut global = HashMap::new();
        global.insert("__builtin_va_list".to_string(), true);
        Parser { toks, pos: 0, scopes: vec![global], diags: Vec::new() }
    }

    // ---------------------------------------------------------------- cursor

    fn peek(&self) -> &Token {
        self.peek_n(0)
    }

    fn peek_n(&self, n: usize) -> &Token {
        let mut i = self.pos;
        let mut left = n;
        loop {
            match self.toks.get(i) {
                None => return self.toks.last().expect("token stream ends with Eof"),
                Some(t) if t.kind == TokenKind::Pragma => i += 1,
                Some(t) => {
                    if left == 0 {
                        return t;
                    }
                    left -= 1;
                    i += 1;
                }
            }
        }
    }

    fn skip_pragmas(&mut self) {
        while self.toks.get(self.pos).map_or(false, |t| t.kind == TokenKind::Pragma) {
            self.pos += 1;
        }
    }

    fn next(&mut self) -> Token {
        self.skip_pragmas();
        let t = self.toks.get(self.pos).cloned().unwrap_or_else(|| self.toks.last().unwrap().clone());
        if !t.is_eof() {
            self.pos += 1;
        }
        t
    }

    fn loc(&self) -> Loc {
        self.peek().loc
    }

    fn is_p(&self, p: Punct) -> bool {
        self.peek().is_punct(p)
    }

    fn is_kw(&self, kw: &str) -> bool {
        self.peek().is_ident(kw)
    }

    fn eat(&mut self, p: Punct) -> bool {
        if self.is_p(p) {
            self.next();
            true
        } else {
            false
        }
    }

    fn eat_kw(&mut self, kw: &str) -> bool {
        if self.is_kw(kw) {
            self.next();
            true
        } else {
            false
        }
    }

    fn error(&mut self, loc: Loc, msg: impl Into<String>) {
        self.diags.push(Diag::error(loc, msg));
    }

    fn expect(&mut self, p: Punct) -> PResult<()> {
        if self.eat(p) {
            Ok(())
        } else {
            let t = self.peek().clone();
            let found = if t.is_eof() { "end of file".to_string() } else { format!("'{}'", t.text) };
            self.error(t.loc, format!("expected '{}' before {}", p.as_str(), found));
            Err(())
        }
    }

    fn ident(&mut self) -> PResult<(String, Loc)> {
        let t = self.peek().clone();
        if t.kind == TokenKind::Ident && !is_keyword(&t.text) {
            self.next();
            Ok((t.text.to_string(), t.loc))
        } else {
            self.error(t.loc, format!("expected identifier before '{}'", t.text));
            Err(())
        }
    }

    // ---------------------------------------------------------------- scopes

    fn push_scope(&mut self) {
        self.scopes.push(HashMap::new());
    }

    fn pop_scope(&mut self) {
        self.scopes.pop();
    }

    fn declare(&mut self, name: &str, is_typedef: bool) {
        self.scopes.last_mut().unwrap().insert(name.to_string(), is_typedef);
    }

    fn is_typedef_name(&self, name: &str) -> bool {
        for s in self.scopes.iter().rev() {
            if let Some(&t) = s.get(name) {
                return t;
            }
        }
        false
    }

    /// Does the token start a declaration specifier / type name?
    fn starts_type(&self, t: &Token) -> bool {
        if t.kind != TokenKind::Ident {
            return false;
        }
        match &*t.text {
            "void" | "char" | "short" | "int" | "long" | "float" | "double" | "signed" | "unsigned" | "_Bool"
            | "_Complex" | "_Imaginary" | "struct" | "union" | "enum" | "const" | "volatile" | "restrict"
            | "_Atomic" | "typeof" | "__typeof__" | "__typeof" | "_Alignas" | "alignas" | "__attribute__"
            | "__attribute" | "__restrict" | "__restrict__" | "__volatile__" | "__volatile" | "__const"
            | "__const__" | "__signed__" | "__signed" | "__extension__" | "__int128" | "__int128_t"
            | "__uint128_t" => true,
            name => !is_keyword(name) && self.is_typedef_name(name),
        }
    }

    fn starts_decl(&self, t: &Token) -> bool {
        if self.starts_type(t) {
            return true;
        }
        matches!(
            &*t.text,
            "typedef" | "extern" | "static" | "auto" | "register" | "inline" | "__inline" | "__inline__"
                | "_Noreturn" | "_Thread_local" | "_Static_assert" | "static_assert"
        ) && t.kind == TokenKind::Ident
    }

    // ---------------------------------------------------------------- top level

    pub fn translation_unit(&mut self) -> TranslationUnit {
        let mut items = Vec::new();
        loop {
            if let Some(t) = self.toks.get(self.pos) {
                if t.kind == TokenKind::Pragma {
                    items.push(ExternalDecl::Pragma(t.text.to_string(), t.loc));
                    self.pos += 1;
                    continue;
                }
            }
            if self.peek().is_eof() {
                break;
            }
            if self.eat(Punct::Semi) {
                continue;
            }
            let start = self.pos;
            match self.external_decl() {
                Ok(mut d) => items.append(&mut d),
                Err(()) => {
                    self.recover_top(start);
                }
            }
        }
        TranslationUnit { items }
    }

    fn recover_top(&mut self, start: usize) {
        if self.pos == start {
            self.next();
        }
        // Skip to a `;` or a `}` at depth zero.
        let mut depth = 0i32;
        loop {
            let t = self.peek().clone();
            if t.is_eof() {
                return;
            }
            self.next();
            if t.is_punct(Punct::LBrace) {
                depth += 1;
            } else if t.is_punct(Punct::RBrace) {
                depth -= 1;
                if depth <= 0 {
                    return;
                }
            } else if t.is_punct(Punct::Semi) && depth <= 0 {
                return;
            }
        }
    }

    fn external_decl(&mut self) -> PResult<Vec<ExternalDecl>> {
        if self.is_kw("_Static_assert") || self.is_kw("static_assert") {
            return Ok(vec![ExternalDecl::StaticAssert(self.static_assert()?)]);
        }
        if self.is_kw("asm") || self.is_kw("__asm__") || self.is_kw("__asm") {
            // Top-level asm("..."); keep nothing.
            self.next();
            self.skip_parens()?;
            self.expect(Punct::Semi)?;
            return Ok(vec![]);
        }
        let loc = self.loc();
        let specs = self.decl_specs(true)?;
        if self.eat(Punct::Semi) {
            return Ok(vec![ExternalDecl::Decl(Declaration { specs, declarators: vec![], loc })]);
        }
        let first = self.declarator(false)?;
        self.skip_attributes()?;
        // Function definition?
        if first.is_function() && (self.is_p(Punct::LBrace) || self.starts_decl(&self.peek().clone())) {
            let is_typedef = specs.storage == Some(Storage::Typedef);
            if let Some(n) = first.name() {
                self.declare(n, is_typedef);
            }
            // K&R parameter declarations.
            let mut old_params = Vec::new();
            while !self.is_p(Punct::LBrace) && !self.peek().is_eof() {
                let dloc = self.loc();
                let ps = self.decl_specs(false)?;
                let mut decls = Vec::new();
                loop {
                    let d = self.declarator(false)?;
                    decls.push(InitDeclarator { declarator: d, init: None });
                    if !self.eat(Punct::Comma) {
                        break;
                    }
                }
                self.expect(Punct::Semi)?;
                old_params.push(Declaration { specs: ps, declarators: decls, loc: dloc });
            }
            self.push_scope();
            self.declare_params(&first);
            let body = self.block_inner()?;
            self.pop_scope();
            return Ok(vec![ExternalDecl::Function(FunctionDef { specs, declarator: first, old_params, body, loc })]);
        }
        let decl = self.finish_declaration(specs, first, loc)?;
        Ok(vec![ExternalDecl::Decl(decl)])
    }

    /// Declares the parameter names of a function declarator in the current
    /// scope (so they shadow typedef names inside the body).
    fn declare_params(&mut self, d: &Declarator) {
        // Find the function derivation closest to the name.
        fn find(d: &Declarator) -> Option<&Declarator> {
            match &d.kind {
                DeclaratorKind::Ident(_) => None,
                DeclaratorKind::Pointer(_, i) => find(i),
                DeclaratorKind::Array { inner, .. } => find(inner),
                DeclaratorKind::Function { inner, .. } => {
                    if matches!(inner.kind, DeclaratorKind::Ident(_)) {
                        Some(d)
                    } else {
                        find(inner).or(Some(d))
                    }
                }
            }
        }
        if let Some(f) = find(d) {
            if let DeclaratorKind::Function { params, old_names, .. } = &f.kind {
                let names: Vec<String> = params.iter().filter_map(|p| p.declarator.name().map(String::from)).collect();
                for n in names {
                    self.declare(&n, false);
                }
                for n in old_names.clone() {
                    self.declare(&n, false);
                }
            }
        }
    }

    fn finish_declaration(&mut self, specs: DeclSpecs, first: Declarator, loc: Loc) -> PResult<Declaration> {
        let is_typedef = specs.storage == Some(Storage::Typedef);
        let mut declarators = Vec::new();
        let mut d = first;
        loop {
            if let Some(n) = d.name() {
                self.declare(n, is_typedef);
            }
            self.skip_attributes()?;
            let init = if self.eat(Punct::Assign) { Some(self.initializer()?) } else { None };
            declarators.push(InitDeclarator { declarator: d, init });
            if !self.eat(Punct::Comma) {
                break;
            }
            d = self.declarator(false)?;
        }
        self.expect(Punct::Semi)?;
        Ok(Declaration { specs, declarators, loc })
    }

    fn static_assert(&mut self) -> PResult<StaticAssert> {
        let loc = self.loc();
        self.next();
        self.expect(Punct::LParen)?;
        let cond = self.assign_expr()?;
        let message = if self.eat(Punct::Comma) {
            let mut parts = Vec::new();
            while self.peek().kind == TokenKind::Str {
                parts.push(self.next().text.to_string());
            }
            Some(parts.join(" "))
        } else {
            None
        };
        self.expect(Punct::RParen)?;
        self.expect(Punct::Semi)?;
        Ok(StaticAssert { cond, message, loc })
    }

    fn skip_parens(&mut self) -> PResult<Vec<String>> {
        self.expect(Punct::LParen)?;
        let mut depth = 1;
        let mut out = Vec::new();
        loop {
            let t = self.next();
            if t.is_eof() {
                self.error(t.loc, "unbalanced parentheses");
                return Err(());
            }
            if t.is_punct(Punct::LParen) {
                depth += 1;
            } else if t.is_punct(Punct::RParen) {
                depth -= 1;
                if depth == 0 {
                    return Ok(out);
                }
            }
            out.push(t.text.to_string());
        }
    }

    /// Skips GNU `__attribute__((...))`, `asm("label")` and C23 `[[...]]`,
    /// returning the attributes.
    fn attributes(&mut self) -> PResult<Vec<Attribute>> {
        let mut attrs = Vec::new();
        loop {
            if self.is_kw("__attribute__") || self.is_kw("__attribute") {
                self.next();
                let toks = self.skip_parens()?;
                // toks: ( a , b(args) ) — split top level names roughly.
                let inner: Vec<String> = toks.into_iter().collect();
                let mut i = 0;
                let inner: Vec<String> = if inner.first().map_or(false, |s| s == "(") {
                    inner[1..inner.len().saturating_sub(1)].to_vec()
                } else {
                    inner
                };
                while i < inner.len() {
                    let name = inner[i].clone();
                    i += 1;
                    let mut args = Vec::new();
                    if i < inner.len() && inner[i] == "(" {
                        let mut depth = 0;
                        while i < inner.len() {
                            if inner[i] == "(" {
                                depth += 1;
                            } else if inner[i] == ")" {
                                depth -= 1;
                                if depth == 0 {
                                    i += 1;
                                    break;
                                }
                            }
                            if !(depth == 1 && inner[i] == "(") {
                                args.push(inner[i].clone());
                            }
                            i += 1;
                        }
                    }
                    if name != "," {
                        attrs.push(Attribute { name: name.trim_matches('_').to_string(), args });
                    }
                    if i < inner.len() && inner[i] == "," {
                        i += 1;
                    }
                }
            } else if self.is_kw("asm") || self.is_kw("__asm__") || self.is_kw("__asm") {
                self.next();
                let toks = self.skip_parens()?;
                attrs.push(Attribute { name: "asm".into(), args: toks });
            } else if self.is_p(Punct::LBracket) && self.peek_n(1).is_punct(Punct::LBracket) {
                self.next();
                self.next();
                let mut depth = 0;
                let mut toks = Vec::new();
                loop {
                    let t = self.next();
                    if t.is_eof() {
                        return Err(());
                    }
                    if t.is_punct(Punct::LBracket) {
                        depth += 1;
                    }
                    if t.is_punct(Punct::RBracket) {
                        if depth == 0 {
                            break;
                        }
                        depth -= 1;
                    }
                    toks.push(t.text.to_string());
                }
                self.expect(Punct::RBracket)?;
                if let Some(first) = toks.first() {
                    attrs.push(Attribute { name: first.clone(), args: toks[1..].to_vec() });
                }
            } else {
                return Ok(attrs);
            }
        }
    }

    fn skip_attributes(&mut self) -> PResult<()> {
        self.attributes().map(|_| ())
    }

    // ---------------------------------------------------------------- declaration specifiers

    fn decl_specs(&mut self, allow_storage: bool) -> PResult<DeclSpecs> {
        let loc = self.loc();
        let mut specs = DeclSpecs {
            storage: None,
            thread_local: false,
            inline: false,
            noreturn: false,
            quals: Quals::default(),
            ty: TypeSpec::Keywords(BaseKeywords::default()),
            align: Vec::new(),
            attrs: Vec::new(),
            loc,
        };
        let mut kw = BaseKeywords::default();
        let mut other: Option<TypeSpec> = None;
        let mut seen_any = false;
        loop {
            let t = self.peek().clone();
            if t.kind != TokenKind::Ident {
                if t.is_punct(Punct::LBracket) && self.peek_n(1).is_punct(Punct::LBracket) {
                    let mut a = self.attributes()?;
                    specs.attrs.append(&mut a);
                    continue;
                }
                break;
            }
            let storage = match &*t.text {
                "typedef" => Some(Storage::Typedef),
                "extern" => Some(Storage::Extern),
                "static" => Some(Storage::Static),
                "auto" => Some(Storage::Auto),
                "register" => Some(Storage::Register),
                _ => None,
            };
            if let Some(s) = storage {
                if !allow_storage && s != Storage::Register {
                    self.error(t.loc, format!("storage class '{}' not allowed here", t.text));
                }
                if specs.storage.is_some() {
                    self.error(t.loc, "multiple storage classes in declaration specifiers");
                }
                specs.storage = Some(s);
                self.next();
                seen_any = true;
                continue;
            }
            match &*t.text {
                "_Thread_local" | "thread_local" => {
                    specs.thread_local = true;
                    self.next();
                }
                "inline" | "__inline" | "__inline__" => {
                    specs.inline = true;
                    self.next();
                }
                "_Noreturn" => {
                    specs.noreturn = true;
                    self.next();
                }
                "const" | "__const" | "__const__" => {
                    specs.quals.is_const = true;
                    self.next();
                }
                "volatile" | "__volatile__" | "__volatile" => {
                    specs.quals.is_volatile = true;
                    self.next();
                }
                "restrict" | "__restrict" | "__restrict__" => {
                    specs.quals.is_restrict = true;
                    self.next();
                }
                "_Atomic" => {
                    self.next();
                    if self.is_p(Punct::LParen) {
                        // _Atomic(type-name) specifier.
                        self.next();
                        let tn = self.type_name()?;
                        self.expect(Punct::RParen)?;
                        other = Some(TypeSpec::TypeofType(Box::new(tn)));
                    }
                    specs.quals.is_atomic = true;
                }
                "__extension__" => {
                    self.next();
                }
                "__attribute__" | "__attribute" => {
                    let mut a = self.attributes()?;
                    specs.attrs.append(&mut a);
                }
                "_Alignas" | "alignas" => {
                    self.next();
                    self.expect(Punct::LParen)?;
                    if self.starts_type(&self.peek().clone()) {
                        let tn = self.type_name()?;
                        specs.align.push(AlignSpec::Type(tn));
                    } else {
                        let e = self.cond_expr()?;
                        specs.align.push(AlignSpec::Expr(e));
                    }
                    self.expect(Punct::RParen)?;
                }
                "void" => {
                    kw.void += 1;
                    self.next();
                }
                "char" => {
                    kw.char += 1;
                    self.next();
                }
                "short" => {
                    kw.short += 1;
                    self.next();
                }
                "int" => {
                    kw.int += 1;
                    self.next();
                }
                "long" => {
                    kw.long += 1;
                    self.next();
                }
                "float" => {
                    kw.float += 1;
                    self.next();
                }
                "double" => {
                    kw.double += 1;
                    self.next();
                }
                "signed" | "__signed__" | "__signed" => {
                    kw.signed += 1;
                    self.next();
                }
                "unsigned" => {
                    kw.unsigned += 1;
                    self.next();
                }
                "_Bool" => {
                    kw.bool_ += 1;
                    self.next();
                }
                "_Complex" | "_Imaginary" => {
                    kw.complex += 1;
                    self.next();
                }
                "__int128" | "__int128_t" => {
                    kw.int128 += 1;
                    self.next();
                }
                "__uint128_t" => {
                    kw.int128 += 1;
                    kw.unsigned += 1;
                    self.next();
                }
                "struct" | "union" => {
                    if other.is_some() {
                        self.error(t.loc, "two or more data types in declaration specifiers");
                    }
                    other = Some(TypeSpec::Struct(self.struct_spec()?));
                }
                "enum" => {
                    if other.is_some() {
                        self.error(t.loc, "two or more data types in declaration specifiers");
                    }
                    other = Some(TypeSpec::Enum(self.enum_spec()?));
                }
                "typeof" | "__typeof__" | "__typeof" => {
                    self.next();
                    self.expect(Punct::LParen)?;
                    if self.starts_type(&self.peek().clone()) {
                        let tn = self.type_name()?;
                        other = Some(TypeSpec::TypeofType(Box::new(tn)));
                    } else {
                        let e = self.expr()?;
                        other = Some(TypeSpec::TypeofExpr(Box::new(e)));
                    }
                    self.expect(Punct::RParen)?;
                }
                name => {
                    // A typedef name is a type specifier only when no other
                    // type specifier has been seen.
                    let any_kw = kw != BaseKeywords::default();
                    if other.is_none() && !any_kw && !is_keyword(name) && self.is_typedef_name(name) {
                        other = Some(TypeSpec::TypedefName(name.to_string()));
                        self.next();
                    } else {
                        break;
                    }
                }
            }
            seen_any = true;
        }
        if !seen_any {
            let t = self.peek().clone();
            self.error(t.loc, format!("expected declaration specifiers before '{}'", t.text));
            return Err(());
        }
        if let Some(o) = other {
            if kw != BaseKeywords::default() {
                self.error(loc, "two or more data types in declaration specifiers");
            }
            specs.ty = o;
        } else {
            specs.ty = TypeSpec::Keywords(kw);
        }
        Ok(specs)
    }

    fn struct_spec(&mut self) -> PResult<StructSpec> {
        let t = self.next();
        let kind = if &*t.text == "struct" { StructKind::Struct } else { StructKind::Union };
        let mut attrs = self.attributes()?;
        let name = if self.peek().kind == TokenKind::Ident && !is_keyword(&self.peek().text) {
            Some(self.next().text.to_string())
        } else {
            None
        };
        let mut members = None;
        if self.eat(Punct::LBrace) {
            let mut ms = Vec::new();
            while !self.is_p(Punct::RBrace) {
                if self.peek().is_eof() {
                    self.error(t.loc, "unterminated struct definition");
                    return Err(());
                }
                if self.eat(Punct::Semi) {
                    continue;
                }
                if self.is_kw("_Static_assert") || self.is_kw("static_assert") {
                    ms.push(MemberDecl::StaticAssert(self.static_assert()?));
                    continue;
                }
                let mloc = self.loc();
                let specs = self.decl_specs(false)?;
                let mut declarators = Vec::new();
                if !self.is_p(Punct::Semi) {
                    loop {
                        let d = if self.is_p(Punct::Colon) { None } else { Some(self.declarator(false)?) };
                        let width = if self.eat(Punct::Colon) { Some(self.cond_expr()?) } else { None };
                        self.skip_attributes()?;
                        declarators.push((d, width));
                        if !self.eat(Punct::Comma) {
                            break;
                        }
                    }
                }
                self.expect(Punct::Semi)?;
                ms.push(MemberDecl::Field { specs, declarators, loc: mloc });
            }
            self.expect(Punct::RBrace)?;
            let mut more = self.attributes()?;
            attrs.append(&mut more);
            members = Some(ms);
        }
        if name.is_none() && members.is_none() {
            self.error(t.loc, "expected '{' or a tag name");
            return Err(());
        }
        Ok(StructSpec { kind, name, members, attrs, loc: t.loc })
    }

    fn enum_spec(&mut self) -> PResult<EnumSpec> {
        let t = self.next();
        self.skip_attributes()?;
        let name = if self.peek().kind == TokenKind::Ident && !is_keyword(&self.peek().text) {
            Some(self.next().text.to_string())
        } else {
            None
        };
        let mut variants = None;
        if self.eat(Punct::LBrace) {
            let mut vs = Vec::new();
            while !self.is_p(Punct::RBrace) {
                let (n, loc) = self.ident()?;
                self.skip_attributes()?;
                let value = if self.eat(Punct::Assign) { Some(self.cond_expr()?) } else { None };
                self.declare(&n, false);
                vs.push(Enumerator { name: n, value, loc });
                if !self.eat(Punct::Comma) {
                    break;
                }
            }
            self.expect(Punct::RBrace)?;
            variants = Some(vs);
        }
        if name.is_none() && variants.is_none() {
            self.error(t.loc, "expected '{' or a tag name");
            return Err(());
        }
        Ok(EnumSpec { name, variants, loc: t.loc })
    }

    fn type_qualifiers(&mut self) -> PResult<Quals> {
        let mut q = Quals::default();
        loop {
            let t = self.peek().clone();
            match &*t.text {
                "const" | "__const" | "__const__" if t.kind == TokenKind::Ident => q.is_const = true,
                "volatile" | "__volatile__" | "__volatile" if t.kind == TokenKind::Ident => q.is_volatile = true,
                "restrict" | "__restrict" | "__restrict__" if t.kind == TokenKind::Ident => q.is_restrict = true,
                "_Atomic" if t.kind == TokenKind::Ident => q.is_atomic = true,
                "__attribute__" | "__attribute" => {
                    self.skip_attributes()?;
                    continue;
                }
                "__extension__" => {}
                _ => return Ok(q),
            }
            self.next();
        }
    }

    // ---------------------------------------------------------------- declarators

    /// Parses a declarator; `abstract_ok` allows (and prefers) an abstract
    /// declarator (type names, parameters).
    fn declarator(&mut self, abstract_ok: bool) -> PResult<Declarator> {
        let loc = self.loc();
        if self.eat(Punct::Star) {
            let q = self.type_qualifiers()?;
            let inner = self.declarator(abstract_ok)?;
            return Ok(Declarator { kind: DeclaratorKind::Pointer(q, Box::new(inner)), loc });
        }
        if self.is_p(Punct::Caret) {
            // Blocks (clang): treat `^` as a pointer.
            self.next();
            let q = self.type_qualifiers()?;
            let inner = self.declarator(abstract_ok)?;
            return Ok(Declarator { kind: DeclaratorKind::Pointer(q, Box::new(inner)), loc });
        }
        self.skip_attributes()?;
        let mut d = self.direct_declarator(abstract_ok)?;
        loop {
            let sloc = self.loc();
            if self.is_p(Punct::LBracket) && !self.peek_n(1).is_punct(Punct::LBracket) {
                self.next();
                let mut is_static = self.eat_kw("static");
                let quals = self.type_qualifiers()?;
                if !is_static {
                    is_static = self.eat_kw("static");
                }
                let mut vla_star = false;
                let size = if self.is_p(Punct::Star) && self.peek_n(1).is_punct(Punct::RBracket) {
                    self.next();
                    vla_star = true;
                    None
                } else if self.is_p(Punct::RBracket) {
                    None
                } else {
                    Some(Box::new(self.assign_expr()?))
                };
                self.expect(Punct::RBracket)?;
                d = Declarator { kind: DeclaratorKind::Array { inner: Box::new(d), size, quals, is_static, vla_star }, loc: sloc };
            } else if self.is_p(Punct::LParen) {
                self.next();
                d = self.function_suffix(d, sloc)?;
            } else {
                break;
            }
        }
        Ok(d)
    }

    fn direct_declarator(&mut self, abstract_ok: bool) -> PResult<Declarator> {
        let loc = self.loc();
        let t = self.peek().clone();
        // After declaration specifiers, an identifier is the declared name
        // even when it is also a typedef name (the specifiers would have
        // taken it as the type otherwise): `int f(int T)`.
        if t.kind == TokenKind::Ident && !is_keyword(&t.text) {
            self.next();
            return Ok(Declarator { kind: DeclaratorKind::Ident(Some(t.text.to_string())), loc });
        }
        if t.kind == TokenKind::Ident && !is_keyword(&t.text) && !abstract_ok {
            // A typedef name being redeclared as an ordinary identifier.
            self.next();
            return Ok(Declarator { kind: DeclaratorKind::Ident(Some(t.text.to_string())), loc });
        }
        if t.is_punct(Punct::LParen) {
            // Nested declarator or (abstract) parameter list?
            let n = self.peek_n(1).clone();
            let nested = if abstract_ok {
                n.is_punct(Punct::Star)
                    || n.is_punct(Punct::Caret)
                    || n.is_punct(Punct::LBracket) && !self.peek_n(2).is_punct(Punct::LBracket)
                    || (n.is_punct(Punct::LParen))
                    || (n.kind == TokenKind::Ident
                        && !is_keyword(&n.text)
                        && !self.is_typedef_name(&n.text))
                    || n.is_ident("__attribute__")
            } else {
                true
            };
            if nested {
                self.next();
                let inner = self.declarator(abstract_ok)?;
                self.expect(Punct::RParen)?;
                return Ok(inner);
            }
        }
        if abstract_ok {
            return Ok(Declarator { kind: DeclaratorKind::Ident(None), loc });
        }
        self.error(t.loc, format!("expected identifier or '(' before '{}'", t.text));
        Err(())
    }

    /// After the `(` of a function declarator.
    fn function_suffix(&mut self, inner: Declarator, loc: Loc) -> PResult<Declarator> {
        let mut params = Vec::new();
        let mut variadic = false;
        let mut unprototyped = false;
        let mut old_names = Vec::new();
        if self.eat(Punct::RParen) {
            unprototyped = true;
        } else if self.is_kw("void") && self.peek_n(1).is_punct(Punct::RParen) {
            self.next();
            self.next();
        } else if self.peek().kind == TokenKind::Ident
            && !is_keyword(&self.peek().text)
            && !self.is_typedef_name(&self.peek().text.clone())
        {
            // K&R identifier list.
            loop {
                let (n, _) = self.ident()?;
                old_names.push(n);
                if !self.eat(Punct::Comma) {
                    break;
                }
            }
            self.expect(Punct::RParen)?;
        } else {
            self.push_scope();
            loop {
                if self.eat(Punct::Ellipsis) {
                    variadic = true;
                    break;
                }
                let ploc = self.loc();
                let specs = match self.decl_specs(false) {
                    Ok(s) => s,
                    Err(()) => {
                        self.pop_scope();
                        return Err(());
                    }
                };
                let d = match self.declarator(true) {
                    Ok(d) => d,
                    Err(()) => {
                        self.pop_scope();
                        return Err(());
                    }
                };
                self.skip_attributes()?;
                if let Some(n) = d.name() {
                    self.declare(n, false);
                }
                params.push(ParamDecl { specs, declarator: d, loc: ploc });
                if !self.eat(Punct::Comma) {
                    break;
                }
            }
            self.pop_scope();
            self.expect(Punct::RParen)?;
        }
        Ok(Declarator { kind: DeclaratorKind::Function { inner: Box::new(inner), params, variadic, unprototyped, old_names }, loc })
    }

    pub fn type_name(&mut self) -> PResult<TypeName> {
        let loc = self.loc();
        let specs = self.decl_specs(false)?;
        let declarator = self.declarator(true)?;
        if declarator.name().is_some() {
            self.error(loc, "unexpected identifier in type name");
        }
        Ok(TypeName { specs, declarator, loc })
    }

    // ---------------------------------------------------------------- initializers

    fn initializer(&mut self) -> PResult<Initializer> {
        if self.is_p(Punct::LBrace) {
            let loc = self.loc();
            self.next();
            let items = self.init_list_items()?;
            Ok(Initializer::List(items, loc))
        } else {
            Ok(Initializer::Expr(self.assign_expr()?))
        }
    }

    /// After `{`: items up to and including `}`.
    fn init_list_items(&mut self) -> PResult<Vec<InitItem>> {
        let mut items = Vec::new();
        while !self.is_p(Punct::RBrace) {
            let mut designators = Vec::new();
            loop {
                if self.is_p(Punct::LBracket) {
                    self.next();
                    let a = self.cond_expr()?;
                    if self.eat(Punct::Ellipsis) {
                        let b = self.cond_expr()?;
                        designators.push(Designator::Range(a, b));
                    } else {
                        designators.push(Designator::Index(a));
                    }
                    self.expect(Punct::RBracket)?;
                } else if self.is_p(Punct::Dot) {
                    self.next();
                    let (n, l) = self.ident()?;
                    designators.push(Designator::Field(n, l));
                } else if designators.is_empty()
                    && self.peek().kind == TokenKind::Ident
                    && self.peek_n(1).is_punct(Punct::Colon)
                {
                    // GNU `field: value`.
                    let (n, l) = self.ident()?;
                    self.next();
                    designators.push(Designator::Field(n, l));
                    break;
                } else {
                    break;
                }
            }
            if !designators.is_empty() && !matches!(designators.last(), Some(Designator::Field(..)) if false) {
                // `=` is required after C designators (optional in old GNU).
                self.eat(Punct::Assign);
            }
            let init = self.initializer()?;
            items.push(InitItem { designators, init });
            if !self.eat(Punct::Comma) {
                break;
            }
        }
        self.expect(Punct::RBrace)?;
        Ok(items)
    }

    // ---------------------------------------------------------------- statements

    fn block_inner(&mut self) -> PResult<Block> {
        let loc = self.loc();
        self.expect(Punct::LBrace)?;
        let mut items = Vec::new();
        loop {
            if let Some(t) = self.toks.get(self.pos) {
                if t.kind == TokenKind::Pragma {
                    items.push(BlockItem::Stmt(Stmt { kind: StmtKind::Pragma(t.text.to_string()), loc: t.loc }));
                    self.pos += 1;
                    continue;
                }
            }
            if self.is_p(Punct::RBrace) {
                break;
            }
            if self.peek().is_eof() {
                self.error(loc, "expected '}' at end of input");
                return Err(());
            }
            let start = self.pos;
            match self.block_item() {
                Ok(i) => items.push(i),
                Err(()) => {
                    // Recover: skip to `;` or `}` at this depth.
                    if self.pos == start {
                        self.next();
                    }
                    let mut depth = 0;
                    loop {
                        let t = self.peek().clone();
                        if t.is_eof() {
                            return Err(());
                        }
                        if t.is_punct(Punct::RBrace) {
                            if depth == 0 {
                                break;
                            }
                            depth -= 1;
                        }
                        if t.is_punct(Punct::LBrace) {
                            depth += 1;
                        }
                        self.next();
                        if t.is_punct(Punct::Semi) && depth == 0 {
                            break;
                        }
                    }
                }
            }
        }
        let end = self.loc();
        self.expect(Punct::RBrace)?;
        Ok(Block { items, loc, end })
    }

    fn block_item(&mut self) -> PResult<BlockItem> {
        if self.is_kw("_Static_assert") || self.is_kw("static_assert") {
            return Ok(BlockItem::StaticAssert(self.static_assert()?));
        }
        let t = self.peek().clone();
        // A label `name:` where name is a typedef is still a label.
        let is_label = t.kind == TokenKind::Ident && self.peek_n(1).is_punct(Punct::Colon) && !is_keyword(&t.text);
        if !is_label && self.starts_decl(&t) {
            let loc = self.loc();
            let specs = self.decl_specs(true)?;
            if self.eat(Punct::Semi) {
                return Ok(BlockItem::Decl(Declaration { specs, declarators: vec![], loc }));
            }
            let first = self.declarator(false)?;
            return Ok(BlockItem::Decl(self.finish_declaration(specs, first, loc)?));
        }
        Ok(BlockItem::Stmt(self.stmt()?))
    }

    fn stmt(&mut self) -> PResult<Stmt> {
        self.skip_pragmas();
        let loc = self.loc();
        let t = self.peek().clone();
        let kind = if t.kind == TokenKind::Ident {
            match &*t.text {
                "if" => {
                    self.next();
                    self.expect(Punct::LParen)?;
                    let c = self.expr()?;
                    self.expect(Punct::RParen)?;
                    let a = self.sub_stmt()?;
                    let b = if self.eat_kw("else") { Some(Box::new(self.sub_stmt()?)) } else { None };
                    StmtKind::If(c, Box::new(a), b)
                }
                "while" => {
                    self.next();
                    self.expect(Punct::LParen)?;
                    let c = self.expr()?;
                    self.expect(Punct::RParen)?;
                    StmtKind::While(c, Box::new(self.sub_stmt()?))
                }
                "do" => {
                    self.next();
                    let body = self.sub_stmt()?;
                    if !self.eat_kw("while") {
                        self.error(self.loc(), "expected 'while' in do/while loop");
                        return Err(());
                    }
                    self.expect(Punct::LParen)?;
                    let c = self.expr()?;
                    self.expect(Punct::RParen)?;
                    self.expect(Punct::Semi)?;
                    StmtKind::DoWhile(Box::new(body), c)
                }
                "for" => {
                    self.next();
                    self.expect(Punct::LParen)?;
                    self.push_scope();
                    let r = (|| -> PResult<StmtKind> {
                        let init = if self.eat(Punct::Semi) {
                            None
                        } else if self.starts_decl(&self.peek().clone()) {
                            let dloc = self.loc();
                            let specs = self.decl_specs(true)?;
                            let first = self.declarator(false)?;
                            Some(ForInit::Decl(self.finish_declaration(specs, first, dloc)?))
                        } else {
                            let e = self.expr()?;
                            self.expect(Punct::Semi)?;
                            Some(ForInit::Expr(e))
                        };
                        let cond = if self.is_p(Punct::Semi) { None } else { Some(self.expr()?) };
                        self.expect(Punct::Semi)?;
                        let step = if self.is_p(Punct::RParen) { None } else { Some(self.expr()?) };
                        self.expect(Punct::RParen)?;
                        let body = self.sub_stmt()?;
                        Ok(StmtKind::For(init, cond, step, Box::new(body)))
                    })();
                    self.pop_scope();
                    r?
                }
                "switch" => {
                    self.next();
                    self.expect(Punct::LParen)?;
                    let c = self.expr()?;
                    self.expect(Punct::RParen)?;
                    StmtKind::Switch(c, Box::new(self.sub_stmt()?))
                }
                "case" => {
                    self.next();
                    let v = self.cond_expr()?;
                    let hi = if self.eat(Punct::Ellipsis) { Some(self.cond_expr()?) } else { None };
                    self.expect(Punct::Colon)?;
                    let s = self.stmt_or_empty_label()?;
                    StmtKind::Case(v, hi, Box::new(s))
                }
                "default" => {
                    self.next();
                    self.expect(Punct::Colon)?;
                    StmtKind::Default(Box::new(self.stmt_or_empty_label()?))
                }
                "break" => {
                    self.next();
                    self.expect(Punct::Semi)?;
                    StmtKind::Break
                }
                "continue" => {
                    self.next();
                    self.expect(Punct::Semi)?;
                    StmtKind::Continue
                }
                "return" => {
                    self.next();
                    let e = if self.is_p(Punct::Semi) { None } else { Some(self.expr()?) };
                    self.expect(Punct::Semi)?;
                    StmtKind::Return(e)
                }
                "goto" => {
                    self.next();
                    if self.eat(Punct::Star) {
                        let e = self.expr()?;
                        self.expect(Punct::Semi)?;
                        StmtKind::GotoExpr(e)
                    } else {
                        let (n, _) = self.ident()?;
                        self.expect(Punct::Semi)?;
                        StmtKind::Goto(n)
                    }
                }
                "asm" | "__asm__" | "__asm" => {
                    self.next();
                    let _ = self.type_qualifiers()?;
                    while self.eat_kw("goto") || self.eat_kw("inline") {}
                    let toks = self.skip_parens()?;
                    self.expect(Punct::Semi)?;
                    StmtKind::Asm(toks.join(" "))
                }
                name if self.peek_n(1).is_punct(Punct::Colon) && !is_keyword(name) => {
                    self.next();
                    self.next();
                    self.skip_attributes()?;
                    StmtKind::Labeled(name.to_string(), Box::new(self.stmt_or_empty_label()?))
                }
                _ => self.expr_stmt()?,
            }
        } else if t.is_punct(Punct::LBrace) {
            self.push_scope();
            let b = self.block_inner();
            self.pop_scope();
            StmtKind::Compound(b?)
        } else {
            self.expr_stmt()?
        };
        Ok(Stmt { kind, loc })
    }

    /// A label may precede `}` in C23; accept it as an empty statement.
    fn stmt_or_empty_label(&mut self) -> PResult<Stmt> {
        if self.is_p(Punct::RBrace) {
            return Ok(Stmt { kind: StmtKind::Expr(None), loc: self.loc() });
        }
        // C23 allows a declaration after a label; wrap it in a block.
        let t = self.peek().clone();
        if self.starts_decl(&t) && !self.peek_n(1).is_punct(Punct::Colon) {
            let loc = self.loc();
            let item = self.block_item()?;
            return Ok(Stmt { kind: StmtKind::Compound(Block { items: vec![item], loc, end: loc }), loc });
        }
        self.stmt()
    }

    fn sub_stmt(&mut self) -> PResult<Stmt> {
        // Each selection/iteration body is its own scope (C99 6.8.4/6.8.5).
        self.push_scope();
        let s = self.stmt();
        self.pop_scope();
        s
    }

    fn expr_stmt(&mut self) -> PResult<StmtKind> {
        if self.eat(Punct::Semi) {
            return Ok(StmtKind::Expr(None));
        }
        let e = self.expr()?;
        self.expect(Punct::Semi)?;
        Ok(StmtKind::Expr(Some(e)))
    }

    // ---------------------------------------------------------------- expressions

    pub fn expr(&mut self) -> PResult<Expr> {
        let mut e = self.assign_expr()?;
        while self.is_p(Punct::Comma) {
            let loc = self.loc();
            self.next();
            let r = self.assign_expr()?;
            e = Expr { kind: ExprKind::Comma(Box::new(e), Box::new(r)), loc };
        }
        Ok(e)
    }

    pub fn assign_expr(&mut self) -> PResult<Expr> {
        let lhs = self.cond_expr()?;
        let t = self.peek().clone();
        let op = match t.kind {
            TokenKind::Punct(Punct::Assign) => Some(None),
            TokenKind::Punct(Punct::StarAssign) => Some(Some(BinaryOp::Mul)),
            TokenKind::Punct(Punct::SlashAssign) => Some(Some(BinaryOp::Div)),
            TokenKind::Punct(Punct::PercentAssign) => Some(Some(BinaryOp::Rem)),
            TokenKind::Punct(Punct::PlusAssign) => Some(Some(BinaryOp::Add)),
            TokenKind::Punct(Punct::MinusAssign) => Some(Some(BinaryOp::Sub)),
            TokenKind::Punct(Punct::ShlAssign) => Some(Some(BinaryOp::Shl)),
            TokenKind::Punct(Punct::ShrAssign) => Some(Some(BinaryOp::Shr)),
            TokenKind::Punct(Punct::AmpAssign) => Some(Some(BinaryOp::BitAnd)),
            TokenKind::Punct(Punct::CaretAssign) => Some(Some(BinaryOp::BitXor)),
            TokenKind::Punct(Punct::PipeAssign) => Some(Some(BinaryOp::BitOr)),
            _ => None,
        };
        if let Some(op) = op {
            self.next();
            let rhs = self.assign_expr()?;
            return Ok(Expr { kind: ExprKind::Assign(op, Box::new(lhs), Box::new(rhs)), loc: t.loc });
        }
        Ok(lhs)
    }

    pub fn cond_expr(&mut self) -> PResult<Expr> {
        let c = self.binary_expr(1)?;
        if self.is_p(Punct::Question) {
            let loc = self.loc();
            self.next();
            // GNU `a ?: b`.
            let a = if self.is_p(Punct::Colon) { c.clone() } else { self.expr()? };
            self.expect(Punct::Colon)?;
            let b = self.cond_expr()?;
            return Ok(Expr { kind: ExprKind::Cond(Box::new(c), Box::new(a), Box::new(b)), loc });
        }
        Ok(c)
    }

    fn binop_at(&self) -> Option<BinaryOp> {
        let TokenKind::Punct(p) = self.peek().kind else { return None };
        Some(match p {
            Punct::Star => BinaryOp::Mul,
            Punct::Slash => BinaryOp::Div,
            Punct::Percent => BinaryOp::Rem,
            Punct::Plus => BinaryOp::Add,
            Punct::Minus => BinaryOp::Sub,
            Punct::Shl => BinaryOp::Shl,
            Punct::Shr => BinaryOp::Shr,
            Punct::Lt => BinaryOp::Lt,
            Punct::Gt => BinaryOp::Gt,
            Punct::Le => BinaryOp::Le,
            Punct::Ge => BinaryOp::Ge,
            Punct::EqEq => BinaryOp::Eq,
            Punct::Ne => BinaryOp::Ne,
            Punct::Amp => BinaryOp::BitAnd,
            Punct::Caret => BinaryOp::BitXor,
            Punct::Pipe => BinaryOp::BitOr,
            Punct::AmpAmp => BinaryOp::LogAnd,
            Punct::PipePipe => BinaryOp::LogOr,
            _ => return None,
        })
    }

    fn binary_expr(&mut self, min_prec: u8) -> PResult<Expr> {
        let mut lhs = self.cast_expr()?;
        while let Some(op) = self.binop_at() {
            let prec = op.precedence();
            if prec < min_prec {
                break;
            }
            let loc = self.loc();
            self.next();
            let rhs = self.binary_expr(prec + 1)?;
            lhs = Expr { kind: ExprKind::Binary(op, Box::new(lhs), Box::new(rhs)), loc };
        }
        Ok(lhs)
    }

    fn cast_expr(&mut self) -> PResult<Expr> {
        if self.is_p(Punct::LParen) && self.starts_type(&self.peek_n(1).clone()) {
            let loc = self.loc();
            self.next();
            let tn = self.type_name()?;
            self.expect(Punct::RParen)?;
            if self.is_p(Punct::LBrace) {
                self.next();
                let items = self.init_list_items()?;
                let lit = Expr { kind: ExprKind::CompoundLiteral(Box::new(tn), items), loc };
                return self.postfix_tail(lit);
            }
            let e = self.cast_expr()?;
            return Ok(Expr { kind: ExprKind::Cast(Box::new(tn), Box::new(e)), loc });
        }
        self.unary_expr()
    }

    fn unary_expr(&mut self) -> PResult<Expr> {
        let t = self.peek().clone();
        let loc = t.loc;
        let op = match t.kind {
            TokenKind::Punct(Punct::Plus) => Some(UnaryOp::Plus),
            TokenKind::Punct(Punct::Minus) => Some(UnaryOp::Neg),
            TokenKind::Punct(Punct::Bang) => Some(UnaryOp::Not),
            TokenKind::Punct(Punct::Tilde) => Some(UnaryOp::BitNot),
            TokenKind::Punct(Punct::Star) => Some(UnaryOp::Deref),
            TokenKind::Punct(Punct::Amp) => Some(UnaryOp::AddrOf),
            _ => None,
        };
        if let Some(op) = op {
            self.next();
            let e = self.cast_expr()?;
            return Ok(Expr { kind: ExprKind::Unary(op, Box::new(e)), loc });
        }
        if t.is_punct(Punct::PlusPlus) || t.is_punct(Punct::MinusMinus) {
            self.next();
            let e = self.unary_expr()?;
            let op = if t.is_punct(Punct::PlusPlus) { UnaryOp::PreInc } else { UnaryOp::PreDec };
            return Ok(Expr { kind: ExprKind::Unary(op, Box::new(e)), loc });
        }
        if t.is_punct(Punct::AmpAmp) {
            // GNU label address `&&label`.
            self.next();
            let (n, _) = self.ident()?;
            self.error(loc, format!("label addresses (&&{}) are not supported", n));
            return Err(());
        }
        if t.kind == TokenKind::Ident {
            match &*t.text {
                "sizeof" => {
                    self.next();
                    if self.is_p(Punct::LParen) && self.starts_type(&self.peek_n(1).clone()) {
                        self.next();
                        let tn = self.type_name()?;
                        self.expect(Punct::RParen)?;
                        if self.is_p(Punct::LBrace) {
                            // sizeof (T){...}
                            self.next();
                            let items = self.init_list_items()?;
                            let lit = Expr { kind: ExprKind::CompoundLiteral(Box::new(tn), items), loc };
                            let e = self.postfix_tail(lit)?;
                            return Ok(Expr { kind: ExprKind::SizeofExpr(Box::new(e)), loc });
                        }
                        return Ok(Expr { kind: ExprKind::SizeofType(Box::new(tn)), loc });
                    }
                    let e = self.unary_expr()?;
                    return Ok(Expr { kind: ExprKind::SizeofExpr(Box::new(e)), loc });
                }
                "_Alignof" | "__alignof__" | "__alignof" | "alignof" => {
                    self.next();
                    if self.is_p(Punct::LParen) && self.starts_type(&self.peek_n(1).clone()) {
                        self.next();
                        let tn = self.type_name()?;
                        self.expect(Punct::RParen)?;
                        return Ok(Expr { kind: ExprKind::AlignofType(Box::new(tn)), loc });
                    }
                    let e = self.unary_expr()?;
                    return Ok(Expr { kind: ExprKind::AlignofExpr(Box::new(e)), loc });
                }
                "__extension__" => {
                    self.next();
                    return self.cast_expr();
                }
                _ => {}
            }
        }
        self.postfix_expr()
    }

    fn postfix_expr(&mut self) -> PResult<Expr> {
        let e = self.primary_expr()?;
        self.postfix_tail(e)
    }

    fn postfix_tail(&mut self, mut e: Expr) -> PResult<Expr> {
        loop {
            let t = self.peek().clone();
            let loc = t.loc;
            match t.kind {
                TokenKind::Punct(Punct::LBracket) => {
                    self.next();
                    let i = self.expr()?;
                    self.expect(Punct::RBracket)?;
                    e = Expr { kind: ExprKind::Index(Box::new(e), Box::new(i)), loc };
                }
                TokenKind::Punct(Punct::LParen) => {
                    self.next();
                    let mut args = Vec::new();
                    if !self.is_p(Punct::RParen) {
                        loop {
                            args.push(self.assign_expr()?);
                            if !self.eat(Punct::Comma) {
                                break;
                            }
                        }
                    }
                    self.expect(Punct::RParen)?;
                    e = Expr { kind: ExprKind::Call(Box::new(e), args), loc };
                }
                TokenKind::Punct(Punct::Dot) | TokenKind::Punct(Punct::Arrow) => {
                    self.next();
                    let (n, _) = self.ident()?;
                    e = Expr { kind: ExprKind::Member(Box::new(e), n, t.is_punct(Punct::Arrow)), loc };
                }
                TokenKind::Punct(Punct::PlusPlus) => {
                    self.next();
                    e = Expr { kind: ExprKind::Unary(UnaryOp::PostInc, Box::new(e)), loc };
                }
                TokenKind::Punct(Punct::MinusMinus) => {
                    self.next();
                    e = Expr { kind: ExprKind::Unary(UnaryOp::PostDec, Box::new(e)), loc };
                }
                _ => return Ok(e),
            }
        }
    }

    fn primary_expr(&mut self) -> PResult<Expr> {
        let t = self.peek().clone();
        let loc = t.loc;
        match t.kind {
            TokenKind::Number => {
                self.next();
                Ok(Expr { kind: ExprKind::Number(t.text.to_string()), loc })
            }
            TokenKind::Char => {
                self.next();
                Ok(Expr { kind: ExprKind::Char(t.text.to_string()), loc })
            }
            TokenKind::Str => {
                let mut parts = Vec::new();
                while self.peek().kind == TokenKind::Str {
                    parts.push(self.next().text.to_string());
                }
                Ok(Expr { kind: ExprKind::Str(parts), loc })
            }
            TokenKind::Punct(Punct::LParen) => {
                self.next();
                if self.is_p(Punct::LBrace) {
                    // GNU statement expression.
                    self.push_scope();
                    let b = self.block_inner();
                    self.pop_scope();
                    let b = b?;
                    self.expect(Punct::RParen)?;
                    return Ok(Expr { kind: ExprKind::StmtExpr(b), loc });
                }
                let e = self.expr()?;
                self.expect(Punct::RParen)?;
                Ok(e)
            }
            TokenKind::Ident => {
                match &*t.text {
                    "_Generic" => {
                        self.next();
                        self.expect(Punct::LParen)?;
                        let c = self.assign_expr()?;
                        let mut assocs = Vec::new();
                        while self.eat(Punct::Comma) {
                            let ty = if self.eat_kw("default") { None } else { Some(self.type_name()?) };
                            self.expect(Punct::Colon)?;
                            let e = self.assign_expr()?;
                            assocs.push((ty, e));
                        }
                        self.expect(Punct::RParen)?;
                        return Ok(Expr { kind: ExprKind::Generic(Box::new(c), assocs), loc });
                    }
                    "__builtin_offsetof" => {
                        self.next();
                        self.expect(Punct::LParen)?;
                        let tn = self.type_name()?;
                        self.expect(Punct::Comma)?;
                        let mut ds = Vec::new();
                        let (n, l) = self.ident()?;
                        ds.push(Designator::Field(n, l));
                        loop {
                            if self.eat(Punct::Dot) {
                                let (n, l) = self.ident()?;
                                ds.push(Designator::Field(n, l));
                            } else if self.eat(Punct::LBracket) {
                                let e = self.expr()?;
                                self.expect(Punct::RBracket)?;
                                ds.push(Designator::Index(e));
                            } else {
                                break;
                            }
                        }
                        self.expect(Punct::RParen)?;
                        return Ok(Expr { kind: ExprKind::Offsetof(Box::new(tn), ds), loc });
                    }
                    "__builtin_va_arg" => {
                        self.next();
                        self.expect(Punct::LParen)?;
                        let ap = self.assign_expr()?;
                        self.expect(Punct::Comma)?;
                        let tn = self.type_name()?;
                        self.expect(Punct::RParen)?;
                        return Ok(Expr { kind: ExprKind::VaArg(Box::new(ap), Box::new(tn)), loc });
                    }
                    name if !is_keyword(name) => {
                        self.next();
                        return Ok(Expr { kind: ExprKind::Ident(name.to_string()), loc });
                    }
                    _ => {}
                }
                self.error(loc, format!("expected expression before '{}'", t.text));
                Err(())
            }
            _ => {
                let what = if t.is_eof() { "end of file".to_string() } else { format!("'{}'", t.text) };
                self.error(loc, format!("expected expression before {}", what));
                Err(())
            }
        }
    }
}
