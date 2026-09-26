//! Declarations, types from specifiers/declarators, and statements.

use crate::hir::{self, GlobalId, Linkage, LocalId, Section, StmtKind};
use crate::types::*;
use loomcc_parse::ast::{self, DeclaratorKind, Storage, TypeSpec};
use loomcc_pp::{Diag, Loc};
use std::collections::{HashMap, HashSet};

#[derive(Clone, Debug)]
pub(crate) enum Sym {
    Local(LocalId),
    Global(GlobalId),
    EnumConst(i64),
    Typedef(Ty),
}

pub(crate) struct FuncState {
    pub locals: Vec<hir::Local>,
    pub ret: Ty,
    pub name: String,
    /// Stack of open switches: collected case values and default.
    pub switches: Vec<(Vec<(i64, i64, u32)>, Option<u32>, Ty)>,
    pub next_case: u32,
    pub loops: u32,
    pub breakable: u32,
    pub labels_defined: HashSet<String>,
    pub labels_used: Vec<(String, Loc)>,
}

pub struct Checker {
    pub types: Types,
    pub globals: Vec<hir::Global>,
    pub(crate) scopes: Vec<HashMap<String, Sym>>,
    pub tags: Vec<HashMap<String, Ty>>,
    pub diags: Vec<Diag>,
    pub(crate) func: Option<FuncState>,
    pub(crate) string_count: u32,
    pub(crate) static_local_count: u32,
    pub unit_name: String,
    pub interrupt_roots: HashSet<String>,
}

pub(crate) struct DeclInfo {
    pub name: Option<String>,
    pub ty: Ty,
    pub loc: Loc,
    /// For a function declarator directly on the name: parameter names and
    /// types (after adjustment), for definitions.
    pub params: Vec<(Option<String>, Ty, Loc)>,
}

impl Checker {
    pub fn new(layout: Layout, unit_name: &str) -> Checker {
        let mut c = Checker {
            types: Types::new(layout),
            globals: Vec::new(),
            scopes: vec![HashMap::new()],
            tags: vec![HashMap::new()],
            diags: Vec::new(),
            func: None,
            string_count: 0,
            static_local_count: 0,
            unit_name: unit_name.to_string(),
            interrupt_roots: HashSet::new(),
        };
        // __builtin_va_list: a pointer-sized opaque type.
        let vl = c.types.ptr(Types::CHAR);
        c.scopes[0].insert("__builtin_va_list".into(), Sym::Typedef(vl));
        c
    }

    pub(crate) fn error(&mut self, loc: Loc, msg: impl Into<String>) {
        self.diags.push(Diag::error(loc, msg));
    }

    pub(crate) fn warn(&mut self, loc: Loc, msg: impl Into<String>) {
        self.diags.push(Diag::warning(loc, msg));
    }

    pub(crate) fn lookup(&self, name: &str) -> Option<Sym> {
        for s in self.scopes.iter().rev() {
            if let Some(sym) = s.get(name) {
                return Some(sym.clone());
            }
        }
        None
    }

    fn lookup_tag(&self, name: &str) -> Option<Ty> {
        for s in self.tags.iter().rev() {
            if let Some(t) = s.get(name) {
                return Some(*t);
            }
        }
        None
    }

    pub(crate) fn push_scope(&mut self) {
        self.scopes.push(HashMap::new());
        self.tags.push(HashMap::new());
    }

    pub(crate) fn pop_scope(&mut self) {
        self.scopes.pop();
        self.tags.pop();
    }

    pub(crate) fn at_file_scope(&self) -> bool {
        self.func.is_none()
    }

    pub fn check_unit(&mut self, tu: &ast::TranslationUnit) {
        for item in &tu.items {
            match item {
                ast::ExternalDecl::Function(f) => self.function_def(f),
                ast::ExternalDecl::Decl(d) => self.declaration(d, &mut Vec::new()),
                ast::ExternalDecl::StaticAssert(s) => self.static_assert(s),
                ast::ExternalDecl::Pragma(p, loc) => self.pragma(p, *loc),
            }
        }
        // Tentative definitions become zero-initialised definitions.
        for g in &mut self.globals {
            if !g.is_func && g.defined && g.init.is_none() {
                if !self.types.is_complete(g.ty) {
                    if let TyKind::Array(e, None) = self.types.kind(g.ty).clone() {
                        // `int a[];` at file scope: one element (C17 6.9.2p5).
                        g.ty = self.types.array(e, Some(1));
                    }
                }
                let size = self.types.size(g.ty) as usize;
                g.init = Some(hir::StaticInit { bytes: vec![0; size], relocs: vec![] });
                g.section = if self.types.is_const(g.ty) { Section::Rodata } else { Section::Bss };
            }
        }
        for name in self.interrupt_roots.clone() {
            if let Some(g) = self.globals.iter_mut().find(|g| g.name == name && g.is_func) {
                if let Some(f) = &mut g.func {
                    f.interrupt = true;
                }
            }
        }
    }

    fn pragma(&mut self, p: &str, _loc: Loc) {
        // `#pragma loomcc interrupt(name)`
        let t: Vec<&str> = p.split(|c: char| c.is_whitespace() || c == '(' || c == ')').filter(|s| !s.is_empty()).collect();
        if t.len() >= 3 && t[0] == "loomcc" && t[1] == "interrupt" {
            for n in &t[2..] {
                self.interrupt_roots.insert(n.to_string());
            }
        }
    }

    pub(crate) fn static_assert(&mut self, s: &ast::StaticAssert) {
        let e = self.expr(&s.cond);
        match self.eval_int(&e) {
            Some(0) => {
                let m = s.message.clone().unwrap_or_default();
                self.error(s.loc, format!("static assertion failed {}", m));
            }
            Some(_) => {}
            None => self.error(s.loc, "static assertion expression is not an integer constant expression"),
        }
    }

    // ------------------------------------------------------------------ types

    /// The type named by declaration specifiers (without declarator).
    pub(crate) fn specs_type(&mut self, specs: &ast::DeclSpecs) -> Ty {
        let base = match &specs.ty {
            TypeSpec::Keywords(k) => self.keyword_type(k, specs.loc),
            TypeSpec::Struct(s) => self.struct_type(s),
            TypeSpec::Enum(e) => self.enum_type(e),
            TypeSpec::TypedefName(n) => match self.lookup(n) {
                Some(Sym::Typedef(t)) => t,
                _ => {
                    self.error(specs.loc, format!("unknown type name '{}'", n));
                    Types::INT
                }
            },
            TypeSpec::TypeofExpr(e) => {
                let e = self.expr(e);
                e.ty
            }
            TypeSpec::TypeofType(t) => self.type_name(t),
        };
        let mut q = 0;
        if specs.quals.is_const {
            q |= Q_CONST;
        }
        if specs.quals.is_volatile {
            q |= Q_VOLATILE;
        }
        self.types.with_quals(base, q)
    }

    fn keyword_type(&mut self, k: &ast::BaseKeywords, loc: Loc) -> Ty {
        let unsigned = k.unsigned > 0;
        if k.signed > 0 && unsigned {
            self.error(loc, "both 'signed' and 'unsigned' in declaration specifiers");
        }
        if k.void > 0 {
            return Types::VOID;
        }
        if k.bool_ > 0 {
            return Types::BOOL;
        }
        if k.float > 0 {
            return Types::FLOAT;
        }
        if k.double > 0 {
            return if k.long > 0 { Types::LDOUBLE } else { Types::DOUBLE };
        }
        if k.char > 0 {
            return if unsigned {
                Types::UCHAR
            } else if k.signed > 0 {
                Types::SCHAR
            } else {
                Types::CHAR
            };
        }
        if k.int128 > 0 {
            self.error(loc, "__int128 is not supported");
            return Types::LLONG;
        }
        if k.short > 0 {
            return if unsigned { Types::USHORT } else { Types::SHORT };
        }
        match k.long {
            0 => {
                if unsigned {
                    Types::UINT
                } else {
                    Types::INT
                }
            }
            1 => {
                if unsigned {
                    Types::ULONG
                } else {
                    Types::LONG
                }
            }
            _ => {
                if unsigned {
                    Types::ULLONG
                } else {
                    Types::LLONG
                }
            }
        }
    }

    fn struct_type(&mut self, s: &ast::StructSpec) -> Ty {
        let is_union = s.kind == ast::StructKind::Union;
        // Reference to an existing tag (`struct S` without a body) resolves
        // through enclosing scopes; a definition or `struct S;` declares in
        // the current scope.
        let existing_here = s.name.as_ref().and_then(|n| self.tags.last().unwrap().get(n).copied());
        let ty = match (&s.name, &s.members) {
            (Some(n), None) => match self.lookup_tag(n) {
                Some(t) => t,
                None => self.new_record(Some(n.clone()), is_union),
            },
            (Some(n), Some(_)) => match existing_here {
                Some(t) if !self.types.is_complete(t) => t,
                Some(_) => {
                    self.error(s.loc, format!("redefinition of '{}'", n));
                    self.new_record(Some(n.clone()), is_union)
                }
                None => self.new_record(Some(n.clone()), is_union),
            },
            (None, _) => self.new_record(None, is_union),
        };
        if let TyKind::Record(r) = self.types.kind(ty).clone() {
            if self.types.records[r.0 as usize].is_union != is_union {
                self.error(s.loc, "use of tag with the wrong kind (struct/union)");
            }
        }
        if let Some(members) = &s.members {
            let mut fields = Vec::new();
            for m in members {
                match m {
                    ast::MemberDecl::StaticAssert(sa) => self.static_assert(sa),
                    ast::MemberDecl::Field { specs, declarators, loc } => {
                        let base = self.specs_type(specs);
                        if declarators.is_empty() {
                            // Anonymous struct/union member.
                            if self.types.is_record(base) {
                                fields.push((None, base, None));
                            } else {
                                self.warn(*loc, "declaration does not declare anything");
                            }
                            continue;
                        }
                        for (d, width) in declarators {
                            let (name, fty, floc) = match d {
                                Some(d) => {
                                    let info = self.declarator(d, base);
                                    (info.name, info.ty, info.loc)
                                }
                                None => (None, base, *loc),
                            };
                            let w = match width {
                                Some(we) => {
                                    let e = self.expr(we);
                                    match self.eval_int(&e) {
                                        Some(v) if v >= 0 && (v as u64) <= self.types.size(fty) * 8 => {
                                            if !self.types.is_integer(fty) {
                                                self.error(floc, "bit-field has non-integer type");
                                            }
                                            Some(v as u32)
                                        }
                                        _ => {
                                            self.error(floc, "invalid bit-field width");
                                            Some(1)
                                        }
                                    }
                                }
                                None => None,
                            };
                            if w.is_none() && !self.types.is_complete(fty) {
                                // Flexible array member.
                                if let TyKind::Array(e, None) = self.types.kind(fty).clone() {
                                    let z = self.types.array(e, Some(0));
                                    fields.push((name, z, None));
                                    continue;
                                }
                                self.error(floc, format!("field has incomplete type '{}'", self.types.display(fty)));
                                continue;
                            }
                            if w == Some(0) && name.is_some() {
                                self.error(floc, "named bit-field has zero width");
                            }
                            fields.push((name, fty, w));
                        }
                    }
                }
            }
            if let TyKind::Record(r) = self.types.kind(ty).clone() {
                self.types.layout_record(r, fields);
            }
        }
        ty
    }

    fn new_record(&mut self, name: Option<String>, is_union: bool) -> Ty {
        let id = RecordId(self.types.records.len() as u32);
        self.types.records.push(Record { is_union, name: name.clone(), complete: false, fields: vec![], size: 0, align: 1 });
        let ty = self.types.intern(TyKind::Record(id), 0);
        if let Some(n) = name {
            self.tags.last_mut().unwrap().insert(n, ty);
        }
        ty
    }

    fn enum_type(&mut self, e: &ast::EnumSpec) -> Ty {
        let ty = match (&e.name, &e.variants) {
            (Some(n), None) => match self.lookup_tag(n) {
                Some(t) => t,
                None => self.new_enum(Some(n.clone())),
            },
            (Some(n), Some(_)) => match self.tags.last().unwrap().get(n).copied() {
                Some(t) => t,
                None => self.new_enum(Some(n.clone())),
            },
            (None, _) => self.new_enum(None),
        };
        if let Some(vs) = &e.variants {
            let mut next: i64 = 0;
            for v in vs {
                if let Some(ve) = &v.value {
                    let x = self.expr(ve);
                    match self.eval_int(&x) {
                        Some(val) => next = val,
                        None => self.error(v.loc, "enumerator value is not an integer constant"),
                    }
                }
                if next < i16::MIN as i64 || next > i16::MAX as i64 {
                    // C17 requires enumerators to fit in int (16 bits here).
                    self.warn(v.loc, format!("enumerator value {} does not fit in a 16-bit int", next));
                }
                self.scopes.last_mut().unwrap().insert(v.name.clone(), Sym::EnumConst(next));
                next += 1;
            }
            if let TyKind::Enum(id) = self.types.kind(ty).clone() {
                self.types.enums[id.0 as usize].complete = true;
            }
        }
        ty
    }

    fn new_enum(&mut self, name: Option<String>) -> Ty {
        let id = EnumId(self.types.enums.len() as u32);
        self.types.enums.push(EnumInfo { name: name.clone(), complete: false });
        let ty = self.types.intern(TyKind::Enum(id), 0);
        if let Some(n) = name {
            self.tags.last_mut().unwrap().insert(n, ty);
        }
        ty
    }

    pub(crate) fn type_name(&mut self, t: &ast::TypeName) -> Ty {
        let base = self.specs_type(&t.specs);
        self.declarator(&t.declarator, base).ty
    }

    /// Applies a declarator to a base type.
    pub(crate) fn declarator(&mut self, d: &ast::Declarator, base: Ty) -> DeclInfo {
        match &d.kind {
            DeclaratorKind::Ident(n) => DeclInfo { name: n.clone(), ty: base, loc: d.loc, params: vec![] },
            DeclaratorKind::Pointer(q, inner) => {
                let p = self.types.ptr(base);
                let mut quals = 0;
                if q.is_const {
                    quals |= Q_CONST;
                }
                if q.is_volatile {
                    quals |= Q_VOLATILE;
                }
                if q.is_restrict {
                    quals |= Q_RESTRICT;
                }
                let p = self.types.with_quals(p, quals);
                self.declarator(inner, p)
            }
            DeclaratorKind::Array { inner, size, .. } => {
                if self.types.is_func(base) {
                    self.error(d.loc, "array of functions");
                }
                let n = match size {
                    None => None,
                    Some(e) => {
                        let x = self.expr(e);
                        match self.eval_int(&x) {
                            Some(v) if v >= 0 => Some(v as u64),
                            Some(_) => {
                                self.error(d.loc, "array has negative size");
                                Some(1)
                            }
                            None => {
                                self.error(d.loc, "variable-length arrays are not supported");
                                Some(1)
                            }
                        }
                    }
                };
                let a = self.types.array(base, n);
                self.declarator(inner, a)
            }
            DeclaratorKind::Function { inner, params, variadic, unprototyped, old_names } => {
                if self.types.is_func(base) || self.types.is_array(base) {
                    self.error(d.loc, "function cannot return a function or an array");
                }
                let mut ptys = Vec::new();
                let mut pinfo = Vec::new();
                // Parameters get their own scope (prototype scope); tags
                // declared there are local to it.
                self.push_scope();
                for p in params {
                    let pb = self.specs_type(&p.specs);
                    let info = self.declarator(&p.declarator, pb);
                    let t = self.adjust_param(info.ty);
                    ptys.push(t);
                    pinfo.push((info.name, t, info.loc));
                }
                self.pop_scope();
                for n in old_names {
                    // K&R: types come from the declaration list (default int).
                    pinfo.push((Some(n.clone()), Types::INT, d.loc));
                }
                let ret = self.types.unqual(base);
                let sig = FuncSig { ret, params: ptys, variadic: *variadic, proto: !*unprototyped && old_names.is_empty() };
                let f = self.types.func(sig);
                let mut info = self.declarator(inner, f);
                if matches!(inner.kind, DeclaratorKind::Ident(_)) {
                    info.params = pinfo;
                }
                info
            }
        }
    }

    /// Parameter type adjustment: arrays to pointers, functions to pointers.
    pub(crate) fn adjust_param(&mut self, t: Ty) -> Ty {
        match self.types.kind(t).clone() {
            TyKind::Array(e, _) => {
                let q = self.types.quals(t);
                let p = self.types.ptr(e);
                self.types.with_quals(p, q)
            }
            TyKind::Func(_) => self.types.ptr(t),
            _ => t,
        }
    }

    // ------------------------------------------------------------------ declarations

    /// Declares or finds the global for a file-scope (or extern block-scope)
    /// name.
    pub(crate) fn declare_global(&mut self, name: &str, ty: Ty, linkage: Linkage, is_func: bool, loc: Loc) -> GlobalId {
        // Same name with linkage: merge (search existing globals).
        let existing = self.globals.iter().position(|g| g.name == name && g.linkage_matches(linkage));
        if let Some(i) = existing {
            let g = &self.globals[i];
            let old = g.ty;
            if !self.types.compatible(old, ty) && !self.composite_ok(old, ty) {
                let (a, b) = (self.types.display(old), self.types.display(ty));
                self.error(loc, format!("conflicting types for '{}' ('{}' vs '{}')", name, b, a));
            } else {
                // Complete an incomplete array type; prefer a prototype.
                let newer = self.composite(old, ty);
                self.globals[i].ty = newer;
            }
            return GlobalId(i as u32);
        }
        self.globals.push(hir::Global {
            name: name.to_string(),
            ty,
            linkage,
            is_func,
            defined: false,
            init: None,
            section: Section::Bss,
            loc,
            address_taken: false,
            func: None,
        });
        GlobalId((self.globals.len() - 1) as u32)
    }

    fn composite_ok(&self, a: Ty, b: Ty) -> bool {
        // Arrays of the same element type, one of unknown size; functions
        // where one side is unprototyped.
        match (self.types.kind(a), self.types.kind(b)) {
            (TyKind::Array(x, _), TyKind::Array(y, _)) => self.types.compatible(*x, *y),
            _ => false,
        }
    }

    fn composite(&mut self, old: Ty, new: Ty) -> Ty {
        match (self.types.kind(old).clone(), self.types.kind(new).clone()) {
            (TyKind::Array(_, None), TyKind::Array(_, Some(_))) => new,
            (TyKind::Func(f), TyKind::Func(g)) => {
                if !f.proto && g.proto {
                    new
                } else {
                    old
                }
            }
            _ => old,
        }
    }

    pub(crate) fn declaration(&mut self, d: &ast::Declaration, out: &mut Vec<hir::Stmt>) {
        let base = self.specs_type(&d.specs);
        let storage = d.specs.storage;
        if d.declarators.is_empty() {
            return; // struct/enum declaration only
        }
        for id in &d.declarators {
            let info = self.declarator(&id.declarator, base);
            let Some(name) = info.name.clone() else {
                self.error(info.loc, "declaration without a name");
                continue;
            };
            if storage == Some(Storage::Typedef) {
                if id.init.is_some() {
                    self.error(info.loc, "typedef is initialized");
                }
                self.scopes.last_mut().unwrap().insert(name, Sym::Typedef(info.ty));
                continue;
            }
            let is_func = self.types.is_func(info.ty);
            if self.at_file_scope() || storage == Some(Storage::Extern) || (is_func && storage != Some(Storage::Static)) {
                // File scope, or block-scope extern / function declaration.
                let linkage = if storage == Some(Storage::Static) {
                    Linkage::Internal
                } else if !self.at_file_scope() {
                    // Block-scope extern: the prior visible declaration's
                    // linkage, else external.
                    match self.lookup(&name) {
                        Some(Sym::Global(g)) => self.globals[g.0 as usize].linkage,
                        _ => Linkage::External,
                    }
                } else if storage == Some(Storage::Extern) {
                    match self.scopes[0].get(&name) {
                        Some(Sym::Global(g)) => self.globals[g.0 as usize].linkage,
                        _ => Linkage::External,
                    }
                } else {
                    // A prior `static` declaration keeps internal linkage
                    // (C17 6.2.2p5 for functions).
                    match self.scopes[0].get(&name) {
                        Some(Sym::Global(g)) if is_func => self.globals[g.0 as usize].linkage,
                        _ => Linkage::External,
                    }
                };
                let gid = self.declare_global(&name, info.ty, linkage, is_func, info.loc);
                self.scopes.last_mut().unwrap().insert(name.clone(), Sym::Global(gid));
                if is_func {
                    if id.init.is_some() {
                        self.error(info.loc, format!("function '{}' is initialized like a variable", name));
                    }
                    continue;
                }
                let is_definition = self.at_file_scope() && (storage != Some(Storage::Extern) || id.init.is_some());
                if is_definition {
                    self.globals[gid.0 as usize].defined = true;
                }
                if let Some(init) = &id.init {
                    if !self.at_file_scope() {
                        self.error(info.loc, "'extern' variable has an initializer");
                        continue;
                    }
                    if self.globals[gid.0 as usize].init.is_some() {
                        self.error(info.loc, format!("redefinition of '{}'", name));
                    }
                    self.static_initializer(gid, init);
                }
                continue;
            }
            // Block scope object.
            if storage == Some(Storage::Static) {
                // A static local is a global with a unique name.
                self.static_local_count += 1;
                let fname = self.func.as_ref().map(|f| f.name.clone()).unwrap_or_default();
                let gname = format!("{}.{}.{}", fname, name, self.static_local_count);
                let gid = self.declare_global(&gname, info.ty, Linkage::Internal, false, info.loc);
                self.globals[gid.0 as usize].defined = true;
                self.scopes.last_mut().unwrap().insert(name.clone(), Sym::Global(gid));
                if let Some(init) = &id.init {
                    self.static_initializer(gid, init);
                }
                continue;
            }
            let mut ty = info.ty;
            if self.types.is_void(ty) {
                self.error(info.loc, format!("variable '{}' has incomplete type 'void'", name));
                ty = Types::INT;
            }
            // Complete `int a[] = {...}` from the initializer.
            if let (TyKind::Array(e, None), Some(init)) = (self.types.kind(ty).clone(), &id.init) {
                let n = self.array_len_from_init(e, init);
                ty = self.types.array(e, Some(n));
            }
            if !self.types.is_complete(ty) {
                self.error(info.loc, format!("variable '{}' has incomplete type '{}'", name, self.types.display(ty)));
                ty = Types::INT;
            }
            let lid = self.new_local(&name, ty, false, info.loc);
            let mut init_stmts = Vec::new();
            if let Some(init) = &id.init {
                init_stmts = self.local_initializer(lid, ty, init, info.loc);
            }
            out.push(hir::Stmt { kind: StmtKind::Decl(lid, init_stmts), loc: info.loc });
        }
    }

    pub(crate) fn new_local(&mut self, name: &str, ty: Ty, is_param: bool, loc: Loc) -> LocalId {
        let f = self.func.as_mut().expect("local outside a function");
        f.locals.push(hir::Local { name: name.to_string(), ty, is_param, address_taken: false, static_global: None, loc });
        let id = LocalId((f.locals.len() - 1) as u32);
        if !name.is_empty() {
            self.scopes.last_mut().unwrap().insert(name.to_string(), Sym::Local(id));
        }
        id
    }

    fn function_def(&mut self, f: &ast::FunctionDef) {
        let base = self.specs_type(&f.specs);
        let mut info = self.declarator(&f.declarator, base);
        let Some(name) = info.name.clone() else {
            self.error(f.loc, "function definition without a name");
            return;
        };
        // K&R parameter types.
        if !f.old_params.is_empty() {
            let mut types_by_name: HashMap<String, Ty> = HashMap::new();
            for d in &f.old_params {
                let b = self.specs_type(&d.specs);
                for id in &d.declarators {
                    let pi = self.declarator(&id.declarator, b);
                    if let Some(n) = pi.name {
                        let t = self.adjust_param(pi.ty);
                        types_by_name.insert(n, t);
                    }
                }
            }
            for p in &mut info.params {
                if let Some(n) = &p.0 {
                    if let Some(t) = types_by_name.get(n) {
                        p.1 = *t;
                    }
                }
            }
            if let TyKind::Func(mut sig) = self.types.kind(info.ty).clone() {
                sig.params = info.params.iter().map(|p| p.1).collect();
                sig.proto = false;
                info.ty = self.types.func(sig);
            }
        }
        let Some(sig) = self.types.sig(info.ty).cloned() else {
            self.error(f.loc, "function definition declarator is not a function");
            return;
        };
        let linkage = if f.specs.storage == Some(Storage::Static) {
            Linkage::Internal
        } else {
            match self.scopes[0].get(&name) {
                Some(Sym::Global(g)) => self.globals[g.0 as usize].linkage,
                _ => Linkage::External,
            }
        };
        let gid = self.declare_global(&name, info.ty, linkage, true, info.loc);
        self.scopes[0].insert(name.clone(), Sym::Global(gid));
        if self.globals[gid.0 as usize].defined {
            self.error(info.loc, format!("redefinition of '{}'", name));
        }
        self.globals[gid.0 as usize].defined = true;
        self.func = Some(FuncState {
            locals: Vec::new(),
            ret: sig.ret,
            name: name.clone(),
            switches: Vec::new(),
            next_case: 0,
            loops: 0,
            breakable: 0,
            labels_defined: HashSet::new(),
            labels_used: Vec::new(),
        });
        self.push_scope();
        let mut params = Vec::new();
        for (pname, pty, ploc) in &info.params {
            if self.types.is_void(*pty) {
                self.error(*ploc, "parameter has type void");
            }
            let lid = self.new_local(pname.as_deref().unwrap_or(""), *pty, true, *ploc);
            params.push(lid);
        }
        // __func__
        let body = self.block(&f.body, false);
        self.pop_scope();
        let st = self.func.take().unwrap();
        for (l, loc) in &st.labels_used {
            if !st.labels_defined.contains(l) {
                self.error(*loc, format!("use of undeclared label '{}'", l));
            }
        }
        let func = hir::Function { params, locals: st.locals, body, variadic: sig.variadic, interrupt: false };
        self.globals[gid.0 as usize].func = Some(func);
    }

    // ------------------------------------------------------------------ statements

    pub(crate) fn block(&mut self, b: &ast::Block, new_scope: bool) -> hir::Stmt {
        if new_scope {
            self.push_scope();
        }
        let mut out = Vec::new();
        for item in &b.items {
            match item {
                ast::BlockItem::Decl(d) => self.declaration(d, &mut out),
                ast::BlockItem::StaticAssert(s) => self.static_assert(s),
                ast::BlockItem::Stmt(s) => {
                    let st = self.stmt(s);
                    out.push(st);
                }
            }
        }
        if new_scope {
            self.pop_scope();
        }
        hir::Stmt { kind: StmtKind::Block(out), loc: b.loc }
    }

    fn cond(&mut self, e: &ast::Expr) -> hir::Expr {
        let x = self.expr(e);
        let x = self.rvalue(x);
        if !self.types.is_scalar(x.ty) {
            self.error(e.loc, format!("used type '{}' where a scalar is required", self.types.display(x.ty)));
        }
        x
    }

    pub(crate) fn stmt(&mut self, s: &ast::Stmt) -> hir::Stmt {
        let loc = s.loc;
        let kind = match &s.kind {
            ast::StmtKind::Compound(b) => return self.block(b, true),
            ast::StmtKind::Expr(None) => StmtKind::Empty,
            ast::StmtKind::Expr(Some(e)) => {
                let x = self.expr(e);
                let x = self.rvalue_or_void(x);
                StmtKind::Expr(x)
            }
            ast::StmtKind::If(c, a, b) => {
                let c = self.cond(c);
                let a = self.scoped_stmt(a);
                let b = b.as_ref().map(|b| Box::new(self.scoped_stmt(b)));
                StmtKind::If(c, Box::new(a), b)
            }
            ast::StmtKind::While(c, body) => {
                let c = self.cond(c);
                let body = self.loop_body(body);
                StmtKind::While(c, Box::new(body))
            }
            ast::StmtKind::DoWhile(body, c) => {
                let body = self.loop_body(body);
                let c = self.cond(c);
                StmtKind::DoWhile(Box::new(body), c)
            }
            ast::StmtKind::For(init, c, step, body) => {
                self.push_scope();
                let init = match init {
                    None => None,
                    Some(ast::ForInit::Decl(d)) => {
                        let mut v = Vec::new();
                        self.declaration(d, &mut v);
                        Some(Box::new(hir::Stmt { kind: StmtKind::Block(v), loc }))
                    }
                    Some(ast::ForInit::Expr(e)) => {
                        let x = self.expr(e);
                        let x = self.rvalue_or_void(x);
                        Some(Box::new(hir::Stmt { kind: StmtKind::Expr(x), loc }))
                    }
                };
                let c = c.as_ref().map(|c| self.cond(c));
                let step = step.as_ref().map(|e| {
                    let x = self.expr(e);
                    self.rvalue_or_void(x)
                });
                let body = self.loop_body(body);
                self.pop_scope();
                StmtKind::For(init, c, step, Box::new(body))
            }
            ast::StmtKind::Switch(c, body) => {
                let x = self.expr(c);
                let x = self.rvalue(x);
                if !self.types.is_integer(x.ty) {
                    self.error(c.loc, "statement requires expression of integer type");
                }
                let pt = self.promote(x.ty);
                let x = self.convert(x, pt);
                self.func.as_mut().unwrap().switches.push((Vec::new(), None, pt));
                self.func.as_mut().unwrap().breakable += 1;
                let body = self.scoped_stmt(body);
                self.func.as_mut().unwrap().breakable -= 1;
                let (cases, default, _) = self.func.as_mut().unwrap().switches.pop().unwrap();
                StmtKind::Switch(x, Box::new(body), cases, default)
            }
            ast::StmtKind::Case(v, hi, body) => {
                let lo_e = self.expr(v);
                let lo = self.eval_int(&lo_e);
                let hi_v = match hi {
                    Some(h) => {
                        let he = self.expr(h);
                        self.eval_int(&he)
                    }
                    None => lo,
                };
                let idx = self.func.as_ref().unwrap().next_case;
                match (lo, hi_v, self.func.as_ref().unwrap().switches.is_empty()) {
                    (_, _, true) => self.error(loc, "'case' statement not in switch statement"),
                    (Some(lo), Some(hi), false) => {
                        let st = self.func.as_ref().unwrap().switches.last().unwrap().2;
                        let (lo, hi) = (self.wrap_to(lo, st), self.wrap_to(hi, st));
                        let dup = self.func.as_ref().unwrap().switches.last().unwrap().0.iter().any(|&(a, b, _)| lo <= b && a <= hi);
                        if dup {
                            self.error(loc, format!("duplicate case value '{}'", lo));
                        }
                        let f = self.func.as_mut().unwrap();
                        f.switches.last_mut().unwrap().0.push((lo, hi, idx));
                        f.next_case += 1;
                    }
                    _ => self.error(loc, "case label does not reduce to an integer constant"),
                }
                let body = self.stmt(body);
                StmtKind::Block(vec![hir::Stmt { kind: StmtKind::CaseLabel(idx), loc }, body])
            }
            ast::StmtKind::Default(body) => {
                let idx = self.func.as_ref().unwrap().next_case;
                let f = self.func.as_mut().unwrap();
                if f.switches.is_empty() {
                    self.error(loc, "'default' statement not in switch statement");
                } else {
                    if f.switches.last().unwrap().1.is_some() {
                        self.error(loc, "multiple default labels in one switch");
                    }
                    let f = self.func.as_mut().unwrap();
                    f.switches.last_mut().unwrap().1 = Some(idx);
                    f.next_case += 1;
                }
                let body = self.stmt(body);
                StmtKind::Block(vec![hir::Stmt { kind: StmtKind::CaseLabel(idx), loc }, body])
            }
            ast::StmtKind::Labeled(l, body) => {
                let f = self.func.as_mut().unwrap();
                if !f.labels_defined.insert(l.clone()) {
                    self.error(loc, format!("redefinition of label '{}'", l));
                }
                let body = self.stmt(body);
                StmtKind::Block(vec![hir::Stmt { kind: StmtKind::Label(l.clone()), loc }, body])
            }
            ast::StmtKind::Goto(l) => {
                self.func.as_mut().unwrap().labels_used.push((l.clone(), loc));
                StmtKind::Goto(l.clone())
            }
            ast::StmtKind::GotoExpr(_) => {
                self.error(loc, "computed goto is not supported");
                StmtKind::Empty
            }
            ast::StmtKind::Continue => {
                if self.func.as_ref().unwrap().loops == 0 {
                    self.error(loc, "'continue' statement not in loop statement");
                }
                StmtKind::Continue
            }
            ast::StmtKind::Break => {
                if self.func.as_ref().unwrap().breakable == 0 {
                    self.error(loc, "'break' statement not in loop or switch statement");
                }
                StmtKind::Break
            }
            ast::StmtKind::Return(e) => {
                let ret = self.func.as_ref().unwrap().ret;
                match e {
                    None => {
                        if !self.types.is_void(ret) {
                            self.warn(loc, "non-void function should return a value");
                        }
                        StmtKind::Return(None)
                    }
                    Some(e) => {
                        let x = self.expr(e);
                        if self.types.is_void(ret) {
                            let x = self.rvalue_or_void(x);
                            if !self.types.is_void(x.ty) {
                                self.warn(loc, "void function should not return a value");
                            }
                            StmtKind::Expr(x)
                        } else {
                            let x = self.rvalue(x);
                            let x = self.assign_convert(x, ret, loc, "return");
                            StmtKind::Return(Some(x))
                        }
                    }
                }
            }
            ast::StmtKind::Asm(_) => {
                self.error(loc, "inline assembly is not supported");
                StmtKind::Empty
            }
            ast::StmtKind::Pragma(p) => {
                self.pragma(p, loc);
                StmtKind::Empty
            }
        };
        hir::Stmt { kind, loc }
    }

    fn scoped_stmt(&mut self, s: &ast::Stmt) -> hir::Stmt {
        self.push_scope();
        let r = self.stmt(s);
        self.pop_scope();
        r
    }

    fn loop_body(&mut self, s: &ast::Stmt) -> hir::Stmt {
        let f = self.func.as_mut().unwrap();
        f.loops += 1;
        f.breakable += 1;
        let r = self.scoped_stmt(s);
        let f = self.func.as_mut().unwrap();
        f.loops -= 1;
        f.breakable -= 1;
        r
    }

    /// Wraps a constant to the range of an integer type.
    pub(crate) fn wrap_to(&self, v: i64, t: Ty) -> i64 {
        let bits = self.types.bits(t);
        if bits >= 64 {
            return v;
        }
        let m = (1i128 << bits) - 1;
        let u = (v as i128) & m;
        if self.types.is_signed(t) && u >> (bits - 1) != 0 {
            (u - (1i128 << bits)) as i64
        } else {
            u as i64
        }
    }
}

impl hir::Global {
    fn linkage_matches(&self, l: Linkage) -> bool {
        // An internal (static) global and an external one with the same name
        // are different objects in C only in pathological cases; merge by
        // name, keeping the first linkage.
        let _ = l;
        true
    }
}

/// Section for a defined object: const without relocations to RAM → ROM.
pub(crate) fn section_for(types: &Types, ty: Ty, has_nonzero: bool) -> Section {
    if types.is_const(ty) || array_elem_const(types, ty) {
        Section::Rodata
    } else if has_nonzero {
        Section::Data
    } else {
        Section::Bss
    }
}

fn array_elem_const(types: &Types, ty: Ty) -> bool {
    match types.kind(ty) {
        TyKind::Array(e, _) => types.is_const(*e) || array_elem_const(types, *e),
        _ => false,
    }
}
