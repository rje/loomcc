//! Expressions: name resolution, conversions, operator typing.

use crate::check::{Checker, Sym};
use crate::hir::{self, BinOp, CmpOp, ExprKind, UnOp};
use crate::types::*;
use loomcc_parse::ast::{self, BinaryOp, UnaryOp};
use loomcc_pp::Loc;

fn mk(kind: ExprKind, ty: Ty, loc: Loc) -> hir::Expr {
    hir::Expr { kind, ty, loc }
}

impl Checker {
    // ------------------------------------------------------------------ conversions

    /// Integer promotion.
    pub(crate) fn promote(&mut self, t: Ty) -> Ty {
        match self.types.kind(t).clone() {
            TyKind::Bool | TyKind::Enum(_) => Types::INT,
            TyKind::Int(k) => match k {
                IntKind::Char | IntKind::SChar | IntKind::UChar | IntKind::Short => Types::INT,
                // unsigned short does not fit a 16-bit int.
                IntKind::UShort => Types::UINT,
                _ => self.types.unqual(t),
            },
            _ => self.types.unqual(t),
        }
    }

    /// Usual arithmetic conversions: the common type.
    pub(crate) fn usual(&mut self, a: Ty, b: Ty) -> Ty {
        if self.types.is_float(a) || self.types.is_float(b) {
            let rank = |t: &Types, x: Ty| match t.kind(x) {
                TyKind::Float(FloatKind::LongDouble) => 3,
                TyKind::Float(FloatKind::Double) => 2,
                TyKind::Float(FloatKind::Float) => 1,
                _ => 0,
            };
            return if rank(&self.types, a) >= rank(&self.types, b) { self.types.unqual(a) } else { self.types.unqual(b) };
        }
        let a = self.promote(a);
        let b = self.promote(b);
        if a == b {
            return a;
        }
        let (ka, kb) = (self.types.int_kind(a).unwrap(), self.types.int_kind(b).unwrap());
        if ka.is_signed() == kb.is_signed() {
            return if ka.rank() >= kb.rank() { a } else { b };
        }
        let (u, s, ku, ks) = if ka.is_signed() { (b, a, kb, ka) } else { (a, b, ka, kb) };
        if ku.rank() >= ks.rank() {
            return u;
        }
        if self.types.size(s) > self.types.size(u) {
            return s;
        }
        self.types.intern(TyKind::Int(ks.to_unsigned()), 0)
    }

    /// Lvalue conversion: arrays and functions decay, other lvalues are read
    /// (the type loses its qualifiers).
    pub(crate) fn rvalue(&mut self, e: hir::Expr) -> hir::Expr {
        match self.types.kind(e.ty).clone() {
            TyKind::Array(elem, _) => {
                self.mark_address_taken(&e);
                let p = self.types.ptr(elem);
                let loc = e.loc;
                mk(ExprKind::AddrOf(Box::new(e)), p, loc)
            }
            TyKind::Func(_) => {
                self.mark_address_taken(&e);
                let p = self.types.ptr(e.ty);
                let loc = e.loc;
                mk(ExprKind::AddrOf(Box::new(e)), p, loc)
            }
            _ => {
                let t = self.types.unqual(e.ty);
                if t != e.ty && e.is_lvalue() {
                    // Keep the lvalue node (the IR reads it); record the
                    // unqualified value type on a no-op cast only when the
                    // node's own type must stay (volatile reads keep the
                    // qualifier for the IR to see).
                    return e;
                }
                e
            }
        }
    }

    pub(crate) fn rvalue_or_void(&mut self, e: hir::Expr) -> hir::Expr {
        if self.types.is_void(e.ty) {
            e
        } else {
            self.rvalue(e)
        }
    }

    pub(crate) fn mark_address_taken(&mut self, e: &hir::Expr) {
        match &e.kind {
            ExprKind::Local(l) => {
                if let Some(f) = self.func.as_mut() {
                    f.locals[l.0 as usize].address_taken = true;
                }
            }
            ExprKind::Global(g) => {
                self.globals[g.0 as usize].address_taken = true;
            }
            ExprKind::Member(inner, _) | ExprKind::BitField(inner, ..) => self.mark_address_taken(inner),
            ExprKind::CompoundLiteral(l, _) => {
                if let Some(f) = self.func.as_mut() {
                    f.locals[l.0 as usize].address_taken = true;
                }
            }
            _ => {}
        }
    }

    /// Converts an rvalue to `to` (inserting a Cast when the unqualified
    /// types differ).
    pub(crate) fn convert(&mut self, e: hir::Expr, to: Ty) -> hir::Expr {
        let to = self.types.unqual(to);
        let from = self.types.unqual(e.ty);
        if from == to {
            return e;
        }
        // Fold constant integer conversions right away.
        if let ExprKind::IntConst(v) = e.kind {
            if self.types.is_integer(to) {
                let w = if matches!(self.types.kind(to), TyKind::Bool) { (v != 0) as i64 } else { self.wrap_to(v, to) };
                return mk(ExprKind::IntConst(w), to, e.loc);
            }
        }
        let loc = e.loc;
        mk(ExprKind::Cast(Box::new(e)), to, loc)
    }

    pub(crate) fn is_null_constant(&self, e: &hir::Expr) -> bool {
        let e = match &e.kind {
            ExprKind::Cast(inner) if self.types.pointee(e.ty).map_or(false, |p| self.types.is_void(p)) => inner,
            _ => e,
        };
        self.types.is_integer(e.ty) && self.eval_int(e) == Some(0)
    }

    /// Conversion as if by assignment (arguments, returns, initialisers).
    pub(crate) fn assign_convert(&mut self, e: hir::Expr, to: Ty, loc: Loc, what: &str) -> hir::Expr {
        let tu = self.types.unqual(to);
        let fu = self.types.unqual(e.ty);
        if self.types.is_arith(tu) && self.types.is_arith(fu) {
            return self.convert(e, to);
        }
        if self.types.is_ptr(tu) {
            if self.types.is_ptr(fu) {
                let (pt, pf) = (self.types.pointee(tu).unwrap(), self.types.pointee(fu).unwrap());
                let void_either = self.types.is_void(pt) || self.types.is_void(pf);
                if !void_either && !self.types.compatible_unqual(pt, pf) {
                    self.warn(loc, format!(
                        "incompatible pointer types in {} ('{}' from '{}')",
                        what,
                        self.types.display(tu),
                        self.types.display(fu)
                    ));
                } else if self.types.quals(pf) & !self.types.quals(pt) & (Q_CONST | Q_VOLATILE) != 0 {
                    self.warn(loc, format!("{} discards qualifiers from pointer target type", what));
                }
                return self.convert(e, to);
            }
            if self.is_null_constant(&e) {
                return self.convert(e, to);
            }
            if self.types.is_integer(fu) {
                self.warn(loc, format!("{} makes pointer from integer without a cast", what));
                return self.convert(e, to);
            }
        }
        if self.types.is_integer(tu) && self.types.is_ptr(fu) {
            if matches!(self.types.kind(tu), TyKind::Bool) {
                return self.convert(e, to);
            }
            self.warn(loc, format!("{} makes integer from pointer without a cast", what));
            return self.convert(e, to);
        }
        if self.types.is_record(tu) && self.types.compatible_unqual(tu, fu) {
            return e;
        }
        self.error(loc, format!(
            "incompatible types in {} ('{}' from '{}')",
            what,
            self.types.display(tu),
            self.types.display(fu)
        ));
        e
    }

    fn arith_operands(&mut self, a: hir::Expr, b: hir::Expr) -> (hir::Expr, hir::Expr, Ty) {
        let t = self.usual(a.ty, b.ty);
        let a = self.convert(a, t);
        let b = self.convert(b, t);
        (a, b, t)
    }

    // ------------------------------------------------------------------ expressions

    pub fn expr(&mut self, e: &ast::Expr) -> hir::Expr {
        let loc = e.loc;
        match &e.kind {
            ast::ExprKind::Ident(n) => self.ident(n, loc),
            ast::ExprKind::Number(s) => self.number(s, loc),
            ast::ExprKind::Char(s) => match loomcc_pp::expr::char_value(s) {
                Ok((v, prefix)) => {
                    let ty = match prefix.as_str() {
                        "" => Types::INT,
                        "u8" => Types::UCHAR,
                        "u" => Types::USHORT,
                        "U" => Types::ULONG,
                        _ => Types::USHORT, // L: wchar_t
                    };
                    let v = if prefix.is_empty() { self.wrap_to(v, Types::INT) } else { v };
                    mk(ExprKind::IntConst(v), ty, loc)
                }
                Err(m) => {
                    self.error(loc, m);
                    mk(ExprKind::IntConst(0), Types::INT, loc)
                }
            },
            ast::ExprKind::Str(parts) => self.string_literal(parts, loc),
            ast::ExprKind::Unary(op, a) => self.unary(*op, a, loc),
            ast::ExprKind::Binary(op, a, b) => self.binary(*op, a, b, loc),
            ast::ExprKind::Assign(op, a, b) => self.assign(*op, a, b, loc),
            ast::ExprKind::Cond(c, a, b) => self.conditional(c, a, b, loc),
            ast::ExprKind::Comma(a, b) => {
                let a = self.expr(a);
                let a = self.rvalue_or_void(a);
                let b = self.expr(b);
                let b = self.rvalue_or_void(b);
                let t = b.ty;
                mk(ExprKind::Comma(Box::new(a), Box::new(b)), t, loc)
            }
            ast::ExprKind::Call(f, args) => self.call(f, args, loc),
            ast::ExprKind::Index(a, i) => {
                let a = self.expr(a);
                let i = self.expr(i);
                let a = self.rvalue(a);
                let i = self.rvalue(i);
                // Either operand may be the pointer.
                let (p, i) = if self.types.is_ptr(a.ty) { (a, i) } else { (i, a) };
                if !self.types.is_ptr(p.ty) || !self.types.is_integer(i.ty) {
                    self.error(loc, "subscripted value is not an array or pointer");
                    return mk(ExprKind::IntConst(0), Types::INT, loc);
                }
                let sum = self.ptr_add(p, i, false, loc);
                self.deref(sum, loc)
            }
            ast::ExprKind::Member(a, name, arrow) => {
                let a = self.expr(a);
                let obj = if *arrow {
                    let a = self.rvalue(a);
                    if !self.types.is_ptr(a.ty) {
                        self.error(loc, format!("member reference type '{}' is not a pointer", self.types.display(a.ty)));
                        return mk(ExprKind::IntConst(0), Types::INT, loc);
                    }
                    self.deref(a, loc)
                } else {
                    a
                };
                self.member(obj, name, loc)
            }
            ast::ExprKind::Cast(t, a) => {
                let to = self.type_name(t);
                let a = self.expr(a);
                let a = self.rvalue_or_void(a);
                if self.types.is_void(to) {
                    return mk(ExprKind::Cast(Box::new(a)), Types::VOID, loc);
                }
                if !self.types.is_scalar(to) {
                    if self.types.is_record(to) && self.types.compatible_unqual(to, a.ty) {
                        return a;
                    }
                    self.error(loc, format!("cast to non-scalar type '{}'", self.types.display(to)));
                    return a;
                }
                if !self.types.is_scalar(a.ty) {
                    self.error(loc, format!("cannot cast from '{}'", self.types.display(a.ty)));
                    return mk(ExprKind::IntConst(0), to, loc);
                }
                let tu = self.types.unqual(to);
                if tu == self.types.unqual(a.ty) {
                    // Keep an explicit node so the value is not an lvalue.
                    return mk(ExprKind::Cast(Box::new(a)), tu, loc);
                }
                self.convert(a, tu)
            }
            ast::ExprKind::SizeofExpr(a) => {
                let a = self.expr(a);
                if matches!(a.kind, ExprKind::BitField(..)) {
                    self.error(loc, "invalid application of 'sizeof' to a bit-field");
                }
                self.sizeof(a.ty, loc)
            }
            ast::ExprKind::SizeofType(t) => {
                let ty = self.type_name(t);
                self.sizeof(ty, loc)
            }
            ast::ExprKind::AlignofType(t) => {
                let ty = self.type_name(t);
                let a = self.types.align(ty) as i64;
                mk(ExprKind::IntConst(a), Types::UINT, loc)
            }
            ast::ExprKind::AlignofExpr(a) => {
                let a = self.expr(a);
                let v = self.types.align(a.ty) as i64;
                mk(ExprKind::IntConst(v), Types::UINT, loc)
            }
            ast::ExprKind::CompoundLiteral(t, items) => {
                let mut ty = self.type_name(t);
                let init = ast::Initializer::List(items.clone(), loc);
                if let TyKind::Array(e, None) = self.types.kind(ty).clone() {
                    let n = self.array_len_from_init(e, &init);
                    ty = self.types.array(e, Some(n));
                }
                if self.at_file_scope() {
                    // File-scope compound literal: a static object.
                    self.string_count += 1;
                    let name = format!(".compound.{}", self.string_count);
                    let gid = self.declare_global(&name, ty, hir::Linkage::Internal, false, loc);
                    self.globals[gid.0 as usize].defined = true;
                    self.static_initializer(gid, &init);
                    return mk(ExprKind::Global(gid), ty, loc);
                }
                let lid = self.new_local("", ty, false, loc);
                let stmts = self.local_initializer(lid, ty, &init, loc);
                mk(ExprKind::CompoundLiteral(lid, stmts), ty, loc)
            }
            ast::ExprKind::Generic(c, assocs) => {
                let c = self.expr(c);
                let c = self.rvalue(c);
                let ct = self.types.unqual(c.ty);
                let mut default = None;
                for (t, e) in assocs {
                    match t {
                        None => default = Some(e),
                        Some(t) => {
                            let ty = self.type_name(t);
                            if self.types.compatible(ct, ty) {
                                return self.expr(e);
                            }
                        }
                    }
                }
                match default {
                    Some(e) => self.expr(e),
                    None => {
                        self.error(loc, "controlling expression type not compatible with any generic association");
                        mk(ExprKind::IntConst(0), Types::INT, loc)
                    }
                }
            }
            ast::ExprKind::StmtExpr(b) => {
                if self.func.is_none() {
                    self.error(loc, "statement expression outside a function");
                    return mk(ExprKind::IntConst(0), Types::INT, loc);
                }
                self.push_scope();
                let mut stmts = Vec::new();
                let mut last: Option<hir::Expr> = None;
                let n = b.items.len();
                for (i, item) in b.items.iter().enumerate() {
                    match item {
                        ast::BlockItem::Decl(d) => self.declaration(d, &mut stmts),
                        ast::BlockItem::StaticAssert(s) => self.static_assert(s),
                        ast::BlockItem::Stmt(s) => {
                            if i + 1 == n {
                                if let ast::StmtKind::Expr(Some(e)) = &s.kind {
                                    let x = self.expr(e);
                                    last = Some(self.rvalue_or_void(x));
                                    continue;
                                }
                            }
                            let st = self.stmt(s);
                            stmts.push(st);
                        }
                    }
                }
                self.pop_scope();
                let ty = last.as_ref().map_or(Types::VOID, |e| e.ty);
                mk(ExprKind::StmtExpr(stmts, last.map(Box::new)), ty, loc)
            }
            ast::ExprKind::Offsetof(t, ds) => {
                let mut ty = self.type_name(t);
                let mut off: i64 = 0;
                for d in ds {
                    match d {
                        ast::Designator::Field(n, l) => match self.types.find_field(ty, n) {
                            Some((o, f)) => {
                                off += o as i64;
                                ty = f.ty;
                            }
                            None => {
                                self.error(*l, format!("no member named '{}'", n));
                                break;
                            }
                        },
                        ast::Designator::Index(e) => {
                            let x = self.expr(e);
                            let i = self.eval_int(&x).unwrap_or(0);
                            let el = self.types.elem(ty).unwrap_or(Types::CHAR);
                            off += i * self.types.size(el) as i64;
                            ty = el;
                        }
                        ast::Designator::Range(..) => {}
                    }
                }
                mk(ExprKind::IntConst(off), Types::UINT, loc)
            }
            ast::ExprKind::VaArg(ap, t) => {
                let ap = self.expr(ap);
                let ty = self.type_name(t);
                mk(ExprKind::VaArg(Box::new(ap)), ty, loc)
            }
        }
    }

    fn sizeof(&mut self, ty: Ty, loc: Loc) -> hir::Expr {
        if !self.types.is_complete(ty) && !self.types.is_void(ty) {
            self.error(loc, format!("invalid application of 'sizeof' to an incomplete type '{}'", self.types.display(ty)));
        }
        if self.types.is_func(ty) {
            self.error(loc, "invalid application of 'sizeof' to a function type");
        }
        let s = self.types.size(ty) as i64;
        mk(ExprKind::IntConst(s), Types::UINT, loc)
    }

    fn ident(&mut self, n: &str, loc: Loc) -> hir::Expr {
        match self.lookup(n) {
            Some(Sym::Local(l)) => {
                let ty = self.func.as_ref().unwrap().locals[l.0 as usize].ty;
                mk(ExprKind::Local(l), ty, loc)
            }
            Some(Sym::Global(g)) => {
                let ty = self.globals[g.0 as usize].ty;
                mk(ExprKind::Global(g), ty, loc)
            }
            Some(Sym::EnumConst(v)) => mk(ExprKind::IntConst(v), Types::INT, loc),
            Some(Sym::Typedef(_)) => {
                self.error(loc, format!("unexpected type name '{}': expected expression", n));
                mk(ExprKind::IntConst(0), Types::INT, loc)
            }
            None => {
                if n == "__func__" || n == "__FUNCTION__" {
                    let name = self.func.as_ref().map(|f| f.name.clone()).unwrap_or_default();
                    return self.string_literal(&[format!("\"{}\"", name)], loc);
                }
                // Builtins that behave like functions.
                if n.starts_with("__builtin_") {
                    let ft = self.types.func(FuncSig { ret: Types::INT, params: vec![], variadic: true, proto: false });
                    let gid = self.declare_global(n, ft, hir::Linkage::External, true, loc);
                    return mk(ExprKind::Global(gid), ft, loc);
                }
                self.error(loc, format!("use of undeclared identifier '{}'", n));
                mk(ExprKind::IntConst(0), Types::INT, loc)
            }
        }
    }

    fn number(&mut self, s: &str, loc: Loc) -> hir::Expr {
        if let Some((v, suffix)) = loomcc_pp::expr::parse_integer_literal(s) {
            let lower = suffix.to_ascii_lowercase();
            let u = lower.contains('u');
            let longs = lower.matches('l').count();
            let decimal = !(s.starts_with('0') && s.len() > 1);
            // Candidate types in order (C17 6.4.4.1).
            let mut cands: Vec<Ty> = Vec::new();
            let push_signed = |c: &mut Vec<Ty>, t: Ty| c.push(t);
            match (longs, u) {
                (0, false) => {
                    push_signed(&mut cands, Types::INT);
                    if !decimal {
                        cands.push(Types::UINT);
                    }
                    cands.push(Types::LONG);
                    if !decimal {
                        cands.push(Types::ULONG);
                    }
                    cands.push(Types::LLONG);
                    cands.push(Types::ULLONG);
                }
                (0, true) => cands.extend([Types::UINT, Types::ULONG, Types::ULLONG]),
                (1, false) => {
                    cands.push(Types::LONG);
                    if !decimal {
                        cands.push(Types::ULONG);
                    }
                    cands.push(Types::LLONG);
                    cands.push(Types::ULLONG);
                }
                (1, true) => cands.extend([Types::ULONG, Types::ULLONG]),
                (_, false) => cands.extend([Types::LLONG, Types::ULLONG]),
                (_, true) => cands.push(Types::ULLONG),
            }
            for t in cands {
                let bits = self.types.bits(t);
                let max: u64 = if self.types.is_signed(t) { (1u64 << (bits - 1)) - 1 } else if bits == 64 { u64::MAX } else { (1u64 << bits) - 1 };
                if v <= max {
                    return mk(ExprKind::IntConst(v as i64), t, loc);
                }
            }
            self.warn(loc, "integer constant is too large for its type");
            return mk(ExprKind::IntConst(v as i64), Types::ULLONG, loc);
        }
        // Floating constant.
        let lower = s.to_ascii_lowercase();
        let (body, ty) = if lower.ends_with('f') && !lower.starts_with("0x") || (lower.starts_with("0x") && lower.ends_with('f') && lower.contains('p')) {
            (&s[..s.len() - 1], Types::FLOAT)
        } else if lower.ends_with('l') {
            (&s[..s.len() - 1], Types::LDOUBLE)
        } else {
            (s, Types::DOUBLE)
        };
        let v = if body.to_ascii_lowercase().starts_with("0x") {
            parse_hex_float(body)
        } else {
            body.parse::<f64>().ok()
        };
        match v {
            Some(v) => mk(ExprKind::FloatConst(v), ty, loc),
            None => {
                self.error(loc, format!("invalid numeric constant '{}'", s));
                mk(ExprKind::IntConst(0), Types::INT, loc)
            }
        }
    }

    pub(crate) fn string_literal(&mut self, parts: &[String], loc: Loc) -> hir::Expr {
        // Concatenate; the widest prefix wins.
        let mut units: Vec<u32> = Vec::new();
        let mut prefix = String::new();
        for p in parts {
            let q = p.find('"').unwrap_or(0);
            let pre = &p[..q];
            if !pre.is_empty() && pre != "u8" {
                prefix = pre.to_string();
            }
            let body = &p[q + 1..p.len() - 1];
            match loomcc_pp::expr::decode_escapes(body) {
                Ok(u) => units.extend(u),
                Err(m) => self.error(loc, m),
            }
        }
        let (elem, width) = match prefix.as_str() {
            "" => (Types::CHAR, 1),
            "u" | "L" => (Types::USHORT, 2),
            _ => (Types::ULONG, 4),
        };
        let mut bytes = Vec::new();
        if width == 1 {
            for u in &units {
                bytes.push(*u as u8);
            }
            bytes.push(0);
        } else {
            // Re-decode UTF-8 bytes into code points for wide strings.
            let raw: Vec<u8> = units.iter().map(|u| *u as u8).collect();
            let text = String::from_utf8_lossy(&raw);
            for c in text.chars() {
                let v = c as u32;
                for i in 0..width {
                    bytes.push((v >> (8 * i)) as u8);
                }
            }
            bytes.extend(std::iter::repeat(0).take(width));
        }
        let n = bytes.len() as u64 / width as u64;
        let ty = self.types.array(elem, Some(n));
        self.string_count += 1;
        let name = format!(".str.{}", self.string_count);
        let gid = self.declare_global(&name, ty, hir::Linkage::Internal, false, loc);
        let g = &mut self.globals[gid.0 as usize];
        g.defined = true;
        g.init = Some(hir::StaticInit { bytes, relocs: vec![] });
        g.section = hir::Section::Rodata;
        mk(ExprKind::Str(gid), ty, loc)
    }

    pub(crate) fn deref(&mut self, p: hir::Expr, loc: Loc) -> hir::Expr {
        match self.types.pointee(p.ty) {
            Some(t) => {
                if self.types.is_void(t) {
                    // Allowed (e.g. `*(void*)p` in a discarded context), but
                    // the result is not usable as a value.
                }
                // `*&x` → x
                if let ExprKind::AddrOf(inner) = &p.kind {
                    if inner.is_lvalue() && self.types.unqual(inner.ty) == self.types.unqual(t) {
                        let mut x = (**inner).clone();
                        x.ty = t;
                        return x;
                    }
                }
                mk(ExprKind::Deref(Box::new(p)), t, loc)
            }
            None => {
                self.error(loc, format!("indirection requires pointer operand ('{}' invalid)", self.types.display(p.ty)));
                mk(ExprKind::IntConst(0), Types::INT, loc)
            }
        }
    }

    fn member(&mut self, obj: hir::Expr, name: &str, loc: Loc) -> hir::Expr {
        if !self.types.is_record(obj.ty) {
            self.error(loc, format!("member reference base type '{}' is not a structure or union", self.types.display(obj.ty)));
            return mk(ExprKind::IntConst(0), Types::INT, loc);
        }
        if !self.types.is_complete(obj.ty) {
            self.error(loc, format!("incomplete definition of type '{}'", self.types.display(obj.ty)));
            return mk(ExprKind::IntConst(0), Types::INT, loc);
        }
        let Some((off, f)) = self.types.find_field(obj.ty, name) else {
            self.error(loc, format!("no member named '{}' in '{}'", name, self.types.display(obj.ty)));
            return mk(ExprKind::IntConst(0), Types::INT, loc);
        };
        // The member inherits the object's qualifiers.
        let q = self.types.quals(obj.ty);
        let fty = self.types.with_quals(f.ty, q);
        let obj = if obj.is_lvalue() {
            obj
        } else {
            // A member of an rvalue struct (call result): materialise it.
            self.materialize(obj)
        };
        match f.bits {
            Some((bit, width, unit)) => {
                let signed = self.types.is_signed(f.ty);
                mk(ExprKind::BitField(Box::new(obj), off, bit, width, unit, signed), fty, loc)
            }
            None => {
                // Fold nested members into one offset.
                if let ExprKind::Member(inner, o2) = obj.kind {
                    return mk(ExprKind::Member(inner, o2 + off), fty, loc);
                }
                mk(ExprKind::Member(Box::new(obj), off), fty, loc)
            }
        }
    }

    /// Stores an rvalue aggregate in a temporary so it can be addressed.
    fn materialize(&mut self, e: hir::Expr) -> hir::Expr {
        let loc = e.loc;
        let ty = e.ty;
        if self.func.is_none() {
            self.error(loc, "cannot take a member of a non-lvalue here");
            return e;
        }
        let lid = self.new_local("", ty, false, loc);
        let target = mk(ExprKind::Local(lid), ty, loc);
        let assign = mk(ExprKind::Assign(Box::new(target), Box::new(e)), ty, loc);
        let st = hir::Stmt { kind: hir::StmtKind::Expr(assign), loc };
        mk(ExprKind::CompoundLiteral(lid, vec![st]), ty, loc)
    }

    pub(crate) fn ptr_add(&mut self, p: hir::Expr, i: hir::Expr, sub: bool, loc: Loc) -> hir::Expr {
        let pt = self.types.pointee(p.ty).unwrap();
        if !self.types.is_complete(pt) && !self.types.is_void(pt) {
            self.error(loc, format!("arithmetic on a pointer to an incomplete type '{}'", self.types.display(pt)));
        }
        let scale = if self.types.is_void(pt) || self.types.is_func(pt) { 1 } else { self.types.size(pt) };
        let it = self.promote(i.ty);
        let mut i = self.convert(i, it);
        if sub {
            let t = i.ty;
            i = self.fold_unary(UnOp::Neg, i, t, loc);
        }
        let ty = self.types.unqual(p.ty);
        // Fold constant offsets into address constants later; keep simple.
        mk(ExprKind::PtrAdd(Box::new(p), Box::new(i), scale), ty, loc)
    }

    fn fold_unary(&mut self, op: UnOp, a: hir::Expr, ty: Ty, loc: Loc) -> hir::Expr {
        if let ExprKind::IntConst(v) = a.kind {
            let r = match op {
                UnOp::Neg => v.wrapping_neg(),
                UnOp::BitNot => !v,
                UnOp::Not => (v == 0) as i64,
            };
            return mk(ExprKind::IntConst(self.wrap_to(r, ty)), ty, loc);
        }
        mk(ExprKind::Unary(op, Box::new(a)), ty, loc)
    }

    fn unary(&mut self, op: UnaryOp, a: &ast::Expr, loc: Loc) -> hir::Expr {
        match op {
            UnaryOp::AddrOf => {
                let x = self.expr(a);
                if let ExprKind::BitField(..) = x.kind {
                    self.error(loc, "cannot take the address of a bit-field");
                }
                if !x.is_lvalue() && !self.types.is_func(x.ty) {
                    self.error(loc, "cannot take the address of an rvalue");
                    return x;
                }
                self.mark_address_taken(&x);
                // &*p → p
                if let ExprKind::Deref(p) = x.kind {
                    let t = self.types.ptr(x.ty);
                    let mut p = *p;
                    p.ty = t;
                    return p;
                }
                let t = self.types.ptr(x.ty);
                mk(ExprKind::AddrOf(Box::new(x)), t, loc)
            }
            UnaryOp::Deref => {
                let x = self.expr(a);
                let x = self.rvalue(x);
                if self.types.pointee(x.ty).map_or(false, |t| self.types.is_func(t)) {
                    // *fp is the function designator; calls accept both.
                    let t = self.types.pointee(x.ty).unwrap();
                    return mk(ExprKind::Deref(Box::new(x)), t, loc);
                }
                self.deref(x, loc)
            }
            UnaryOp::Plus | UnaryOp::Neg | UnaryOp::BitNot => {
                let x = self.expr(a);
                let x = self.rvalue(x);
                let ok = if op == UnaryOp::BitNot { self.types.is_integer(x.ty) } else { self.types.is_arith(x.ty) };
                if !ok {
                    self.error(loc, format!("invalid argument type '{}' to unary expression", self.types.display(x.ty)));
                    return x;
                }
                let t = self.promote(x.ty);
                let x = self.convert(x, t);
                match op {
                    UnaryOp::Plus => {
                        if x.is_lvalue() {
                            let loc = x.loc;
                            return mk(ExprKind::Cast(Box::new(x)), t, loc);
                        }
                        x
                    }
                    UnaryOp::Neg => {
                        if let ExprKind::FloatConst(v) = x.kind {
                            return mk(ExprKind::FloatConst(-v), t, loc);
                        }
                        self.fold_unary(UnOp::Neg, x, t, loc)
                    }
                    _ => self.fold_unary(UnOp::BitNot, x, t, loc),
                }
            }
            UnaryOp::Not => {
                let x = self.expr(a);
                let x = self.rvalue(x);
                if !self.types.is_scalar(x.ty) {
                    self.error(loc, format!("invalid argument type '{}' to unary expression", self.types.display(x.ty)));
                    return mk(ExprKind::IntConst(0), Types::INT, loc);
                }
                if self.types.is_integer(x.ty) {
                    let t = self.promote(x.ty);
                    let x = self.convert(x, t);
                    return self.fold_unary(UnOp::Not, x, Types::INT, loc);
                }
                mk(ExprKind::Unary(UnOp::Not, Box::new(x)), Types::INT, loc)
            }
            UnaryOp::PreInc | UnaryOp::PreDec | UnaryOp::PostInc | UnaryOp::PostDec => {
                let x = self.expr(a);
                self.check_modifiable(&x, loc);
                let step = if let Some(p) = self.types.pointee(x.ty) {
                    if self.types.is_void(p) { 1 } else { self.types.size(p) }
                } else if self.types.is_arith(x.ty) {
                    1
                } else {
                    self.error(loc, format!("cannot increment value of type '{}'", self.types.display(x.ty)));
                    1
                };
                let inc = matches!(op, UnaryOp::PreInc | UnaryOp::PostInc);
                let prefix = matches!(op, UnaryOp::PreInc | UnaryOp::PreDec);
                let t = self.types.unqual(x.ty);
                mk(ExprKind::IncDec(Box::new(x), inc, prefix, step), t, loc)
            }
        }
    }

    pub(crate) fn check_modifiable(&mut self, x: &hir::Expr, loc: Loc) {
        if !x.is_lvalue() || matches!(x.kind, ExprKind::Str(_)) {
            self.error(loc, "expression is not assignable");
            return;
        }
        if self.types.is_const(x.ty) {
            self.error(loc, "cannot assign to a variable or member with const-qualified type");
        } else if self.types.is_array(x.ty) {
            self.error(loc, "array type is not assignable");
        } else if let Some(r) = self.types.record(x.ty) {
            if has_const_member(&self.types, r) {
                self.error(loc, "cannot assign to a struct with a const-qualified member");
            }
        }
    }

    fn binary(&mut self, op: BinaryOp, a: &ast::Expr, b: &ast::Expr, loc: Loc) -> hir::Expr {
        if matches!(op, BinaryOp::LogAnd | BinaryOp::LogOr) {
            let x = self.expr(a);
            let x = self.rvalue(x);
            let y = self.expr(b);
            let y = self.rvalue(y);
            if !self.types.is_scalar(x.ty) || !self.types.is_scalar(y.ty) {
                self.error(loc, "invalid operands to logical operator");
            }
            if let (Some(p), Some(q)) = (self.int_const(&x), self.int_const(&y)) {
                let r = if op == BinaryOp::LogAnd { p != 0 && q != 0 } else { p != 0 || q != 0 };
                return mk(ExprKind::IntConst(r as i64), Types::INT, loc);
            }
            let k = if op == BinaryOp::LogAnd {
                ExprKind::LogAnd(Box::new(x), Box::new(y))
            } else {
                ExprKind::LogOr(Box::new(x), Box::new(y))
            };
            return mk(k, Types::INT, loc);
        }
        let x = self.expr(a);
        let x = self.rvalue(x);
        let y = self.expr(b);
        let y = self.rvalue(y);
        self.binary_values(op, x, y, loc)
    }

    pub(crate) fn binary_values(&mut self, op: BinaryOp, x: hir::Expr, y: hir::Expr, loc: Loc) -> hir::Expr {
        let (xp, yp) = (self.types.is_ptr(x.ty), self.types.is_ptr(y.ty));
        match op {
            BinaryOp::Add if xp && self.types.is_integer(y.ty) => return self.ptr_add(x, y, false, loc),
            BinaryOp::Add if yp && self.types.is_integer(x.ty) => return self.ptr_add(y, x, false, loc),
            BinaryOp::Sub if xp && self.types.is_integer(y.ty) => return self.ptr_add(x, y, true, loc),
            BinaryOp::Sub if xp && yp => {
                let (pa, pb) = (self.types.pointee(x.ty).unwrap(), self.types.pointee(y.ty).unwrap());
                if !self.types.compatible_unqual(pa, pb) {
                    self.error(loc, "subtracting pointers to incompatible types");
                }
                let scale = if self.types.is_void(pa) { 1 } else { self.types.size(pa).max(1) };
                return mk(ExprKind::PtrDiff(Box::new(x), Box::new(y), scale), Types::INT, loc);
            }
            BinaryOp::Lt | BinaryOp::Gt | BinaryOp::Le | BinaryOp::Ge | BinaryOp::Eq | BinaryOp::Ne => {
                let cop = match op {
                    BinaryOp::Lt => CmpOp::Lt,
                    BinaryOp::Gt => CmpOp::Gt,
                    BinaryOp::Le => CmpOp::Le,
                    BinaryOp::Ge => CmpOp::Ge,
                    BinaryOp::Eq => CmpOp::Eq,
                    _ => CmpOp::Ne,
                };
                if xp || yp {
                    let (x, y) = if xp && yp {
                        (x, y)
                    } else if xp {
                        if !self.is_null_constant(&y) && self.types.is_integer(y.ty) {
                            self.warn(loc, "comparison between pointer and integer");
                        }
                        let t = x.ty;
                        let y = self.convert(y, t);
                        (x, y)
                    } else {
                        if !self.is_null_constant(&x) && self.types.is_integer(x.ty) {
                            self.warn(loc, "comparison between pointer and integer");
                        }
                        let t = y.ty;
                        let x = self.convert(x, t);
                        (x, y)
                    };
                    return mk(ExprKind::Cmp(cop, Box::new(x), Box::new(y)), Types::INT, loc);
                }
                if !self.types.is_arith(x.ty) || !self.types.is_arith(y.ty) {
                    self.error(loc, "invalid operands to comparison");
                    return mk(ExprKind::IntConst(0), Types::INT, loc);
                }
                let (x, y, t) = self.arith_operands(x, y);
                if let (Some(p), Some(q)) = (self.int_const(&x), self.int_const(&y)) {
                    let signed = self.types.is_signed(t);
                    let (p, q) = if signed { (p as i128, q as i128) } else { (p as u64 as i128, q as u64 as i128) };
                    let r = match cop {
                        CmpOp::Eq => p == q,
                        CmpOp::Ne => p != q,
                        CmpOp::Lt => p < q,
                        CmpOp::Le => p <= q,
                        CmpOp::Gt => p > q,
                        CmpOp::Ge => p >= q,
                    };
                    return mk(ExprKind::IntConst(r as i64), Types::INT, loc);
                }
                return mk(ExprKind::Cmp(cop, Box::new(x), Box::new(y)), Types::INT, loc);
            }
            _ => {}
        }
        let bop = match op {
            BinaryOp::Add => BinOp::Add,
            BinaryOp::Sub => BinOp::Sub,
            BinaryOp::Mul => BinOp::Mul,
            BinaryOp::Div => BinOp::Div,
            BinaryOp::Rem => BinOp::Rem,
            BinaryOp::BitAnd => BinOp::And,
            BinaryOp::BitOr => BinOp::Or,
            BinaryOp::BitXor => BinOp::Xor,
            BinaryOp::Shl => BinOp::Shl,
            BinaryOp::Shr => BinOp::Shr,
            _ => unreachable!(),
        };
        let int_only = matches!(bop, BinOp::Rem | BinOp::And | BinOp::Or | BinOp::Xor | BinOp::Shl | BinOp::Shr);
        let ok = if int_only {
            self.types.is_integer(x.ty) && self.types.is_integer(y.ty)
        } else {
            self.types.is_arith(x.ty) && self.types.is_arith(y.ty)
        };
        if !ok {
            self.error(loc, format!(
                "invalid operands to binary expression ('{}' and '{}')",
                self.types.display(x.ty),
                self.types.display(y.ty)
            ));
            return mk(ExprKind::IntConst(0), Types::INT, loc);
        }
        if matches!(bop, BinOp::Shl | BinOp::Shr) {
            let lt = self.promote(x.ty);
            let rt = self.promote(y.ty);
            let x = self.convert(x, lt);
            let y = self.convert(y, rt);
            if let (Some(p), Some(q)) = (self.int_const(&x), self.int_const(&y)) {
                if q >= 0 && q < self.types.bits(lt) as i64 {
                    let v = if bop == BinOp::Shl {
                        p.wrapping_shl(q as u32)
                    } else if self.types.is_signed(lt) {
                        p >> q
                    } else {
                        ((p as u64) & mask(self.types.bits(lt))).wrapping_shr(q as u32) as i64
                    };
                    return mk(ExprKind::IntConst(self.wrap_to(v, lt)), lt, loc);
                }
            }
            return mk(ExprKind::Binary(bop, Box::new(x), Box::new(y)), lt, loc);
        }
        let (x, y, t) = self.arith_operands(x, y);
        if let (Some(p), Some(q)) = (self.int_const(&x), self.int_const(&y)) {
            let signed = self.types.is_signed(t);
            let bits = self.types.bits(t);
            let (up, uq) = ((p as u64) & mask(bits), (q as u64) & mask(bits));
            let v = match bop {
                BinOp::Add => Some(p.wrapping_add(q)),
                BinOp::Sub => Some(p.wrapping_sub(q)),
                BinOp::Mul => Some(p.wrapping_mul(q)),
                BinOp::Div if q != 0 => Some(if signed { p.wrapping_div(q) } else { (up / uq) as i64 }),
                BinOp::Rem if q != 0 => Some(if signed { p.wrapping_rem(q) } else { (up % uq) as i64 }),
                BinOp::And => Some(p & q),
                BinOp::Or => Some(p | q),
                BinOp::Xor => Some(p ^ q),
                _ => None,
            };
            if let Some(v) = v {
                return mk(ExprKind::IntConst(self.wrap_to(v, t)), t, loc);
            }
        }
        mk(ExprKind::Binary(bop, Box::new(x), Box::new(y)), t, loc)
    }

    pub(crate) fn int_const(&self, e: &hir::Expr) -> Option<i64> {
        match e.kind {
            ExprKind::IntConst(v) => Some(v),
            _ => None,
        }
    }

    fn assign(&mut self, op: Option<BinaryOp>, a: &ast::Expr, b: &ast::Expr, loc: Loc) -> hir::Expr {
        let lhs = self.expr(a);
        self.check_modifiable(&lhs, loc);
        let rhs = self.expr(b);
        let rhs = self.rvalue(rhs);
        let lt = self.types.unqual(lhs.ty);
        match op {
            None => {
                let rhs = self.assign_convert(rhs, lt, loc, "assignment");
                mk(ExprKind::Assign(Box::new(lhs), Box::new(rhs)), lt, loc)
            }
            Some(op) => {
                if self.types.is_ptr(lt) && matches!(op, BinaryOp::Add | BinaryOp::Sub) {
                    if !self.types.is_integer(rhs.ty) {
                        self.error(loc, "invalid operands to pointer compound assignment");
                    }
                    let pt = self.types.pointee(lt).unwrap();
                    let scale = if self.types.is_void(pt) { 1 } else { self.types.size(pt) };
                    let it = self.promote(rhs.ty);
                    let rhs = self.convert(rhs, it);
                    return mk(ExprKind::PtrCompoundAssign(Box::new(lhs), Box::new(rhs), scale, op == BinaryOp::Sub), lt, loc);
                }
                let bop = match op {
                    BinaryOp::Add => BinOp::Add,
                    BinaryOp::Sub => BinOp::Sub,
                    BinaryOp::Mul => BinOp::Mul,
                    BinaryOp::Div => BinOp::Div,
                    BinaryOp::Rem => BinOp::Rem,
                    BinaryOp::BitAnd => BinOp::And,
                    BinaryOp::BitOr => BinOp::Or,
                    BinaryOp::BitXor => BinOp::Xor,
                    BinaryOp::Shl => BinOp::Shl,
                    BinaryOp::Shr => BinOp::Shr,
                    _ => unreachable!(),
                };
                let int_only = matches!(bop, BinOp::Rem | BinOp::And | BinOp::Or | BinOp::Xor | BinOp::Shl | BinOp::Shr);
                let ok = if int_only {
                    self.types.is_integer(lt) && self.types.is_integer(rhs.ty)
                } else {
                    self.types.is_arith(lt) && self.types.is_arith(rhs.ty)
                };
                if !ok {
                    self.error(loc, "invalid operands to compound assignment");
                    return lhs;
                }
                // The operation type: shifts use the promoted left type.
                let op_ty = if matches!(bop, BinOp::Shl | BinOp::Shr) { self.promote(lt) } else { self.usual(lt, rhs.ty) };
                let rhs = if matches!(bop, BinOp::Shl | BinOp::Shr) {
                    let rt = self.promote(rhs.ty);
                    self.convert(rhs, rt)
                } else {
                    self.convert(rhs, op_ty)
                };
                mk(ExprKind::CompoundAssign(bop, Box::new(lhs), Box::new(rhs), op_ty), lt, loc)
            }
        }
    }

    fn conditional(&mut self, c: &ast::Expr, a: &ast::Expr, b: &ast::Expr, loc: Loc) -> hir::Expr {
        let c = self.expr(c);
        let c = self.rvalue(c);
        if !self.types.is_scalar(c.ty) {
            self.error(loc, "used type where a scalar is required");
        }
        let x = self.expr(a);
        let x = self.rvalue_or_void(x);
        let y = self.expr(b);
        let y = self.rvalue_or_void(y);
        let (xt, yt) = (self.types.unqual(x.ty), self.types.unqual(y.ty));
        let (x, y, t) = if self.types.is_arith(xt) && self.types.is_arith(yt) {
            let t = self.usual(xt, yt);
            let x = self.convert(x, t);
            let y = self.convert(y, t);
            (x, y, t)
        } else if self.types.is_void(xt) || self.types.is_void(yt) {
            (x, y, Types::VOID)
        } else if self.types.is_ptr(xt) && self.types.is_ptr(yt) {
            // Pointer to void wins; otherwise combine qualifiers.
            let (px, py) = (self.types.pointee(xt).unwrap(), self.types.pointee(yt).unwrap());
            let q = self.types.quals(px) | self.types.quals(py);
            let base = if self.types.is_void(px) || self.types.is_void(py) { Types::VOID } else { self.types.unqual(px) };
            let tq = self.types.with_quals(base, q);
            let t = self.types.ptr(tq);
            let x = self.convert(x, t);
            let y = self.convert(y, t);
            (x, y, t)
        } else if self.types.is_ptr(xt) && self.is_null_constant(&y) {
            let y = self.convert(y, xt);
            (x, y, xt)
        } else if self.types.is_ptr(yt) && self.is_null_constant(&x) {
            let x = self.convert(x, yt);
            (x, y, yt)
        } else if self.types.is_record(xt) && self.types.compatible(xt, yt) {
            (x, y, xt)
        } else {
            if self.types.is_ptr(xt) || self.types.is_ptr(yt) {
                self.warn(loc, "pointer/integer type mismatch in conditional expression");
                let t = if self.types.is_ptr(xt) { xt } else { yt };
                let x = self.convert(x, t);
                let y = self.convert(y, t);
                return mk(ExprKind::Cond(Box::new(c), Box::new(x), Box::new(y)), t, loc);
            }
            self.error(loc, "incompatible operand types in conditional expression");
            (x, y, xt)
        };
        if let Some(cv) = self.int_const(&c) {
            if let (Some(_), Some(_)) = (self.int_const(&x), self.int_const(&y)) {
                return if cv != 0 { x } else { y };
            }
        }
        mk(ExprKind::Cond(Box::new(c), Box::new(x), Box::new(y)), t, loc)
    }

    fn call(&mut self, f: &ast::Expr, args: &[ast::Expr], loc: Loc) -> hir::Expr {
        // Implicit declaration of an unknown function (C89): warn, declare
        // `int f()`.
        let callee = match &f.kind {
            ast::ExprKind::Ident(n) if self.lookup(n).is_none() && !n.starts_with("__builtin_") => {
                self.warn(loc, format!("implicit declaration of function '{}'", n));
                let ft = self.types.func(FuncSig { ret: Types::INT, params: vec![], variadic: false, proto: false });
                let gid = self.declare_global(n, ft, hir::Linkage::External, true, loc);
                self.scopes[0].insert(n.clone(), Sym::Global(gid));
                mk(ExprKind::Global(gid), ft, f.loc)
            }
            _ => self.expr(f),
        };
        // Builtins.
        if let ExprKind::Global(g) = &callee.kind {
            let name = self.globals[g.0 as usize].name.clone();
            match name.as_str() {
                "__builtin_va_start" | "__builtin_va_end" | "__builtin_va_copy" => {
                    let a = args.first().map(|a| self.expr(a));
                    let a = a.unwrap_or_else(|| mk(ExprKind::IntConst(0), Types::INT, loc));
                    let k = if name == "__builtin_va_end" { ExprKind::VaEnd(Box::new(a)) } else { ExprKind::VaStart(Box::new(a)) };
                    return mk(k, Types::VOID, loc);
                }
                "__builtin_expect" => {
                    if let Some(a) = args.first() {
                        let x = self.expr(a);
                        return self.rvalue(x);
                    }
                }
                "__builtin_unreachable" | "__builtin_trap" => {
                    return mk(ExprKind::IntConst(0), Types::VOID, loc);
                }
                "__builtin_constant_p" => {
                    let v = args.first().map(|a| {
                        let x = self.expr(a);
                        self.eval_int(&x).is_some()
                    });
                    return mk(ExprKind::IntConst(v.unwrap_or(false) as i64), Types::INT, loc);
                }
                _ => {}
            }
        }
        let is_designator = matches!(callee.kind, ExprKind::Global(_)) && self.types.is_func(callee.ty);
        let fptr = if is_designator {
            callee
        } else {
            let c = self.rvalue(callee);
            if let Some(p) = self.types.pointee(c.ty) {
                if self.types.is_func(p) {
                    c
                } else {
                    self.error(loc, "called object is not a function or function pointer");
                    return mk(ExprKind::IntConst(0), Types::INT, loc);
                }
            } else if self.types.is_func(c.ty) {
                c
            } else {
                self.error(loc, format!("called object type '{}' is not a function or function pointer", self.types.display(c.ty)));
                return mk(ExprKind::IntConst(0), Types::INT, loc);
            }
        };
        let fty = if self.types.is_func(fptr.ty) { fptr.ty } else { self.types.pointee(fptr.ty).unwrap() };
        let sig = self.types.sig(fty).unwrap().clone();
        let mut out = Vec::new();
        if sig.proto && (args.len() < sig.params.len() || (!sig.variadic && args.len() > sig.params.len())) {
            self.error(loc, format!(
                "too {} arguments to function call, expected {}, have {}",
                if args.len() < sig.params.len() { "few" } else { "many" },
                sig.params.len(),
                args.len()
            ));
        }
        for (i, a) in args.iter().enumerate() {
            let x = self.expr(a);
            let x = self.rvalue(x);
            let x = if sig.proto && i < sig.params.len() {
                let pt = sig.params[i];
                self.assign_convert(x, pt, a.loc, "argument")
            } else {
                // Default argument promotions.
                if matches!(self.types.kind(x.ty), TyKind::Float(FloatKind::Float)) {
                    self.convert(x, Types::DOUBLE)
                } else if self.types.is_integer(x.ty) {
                    let t = self.promote(x.ty);
                    self.convert(x, t)
                } else {
                    x
                }
            };
            out.push(x);
        }
        // SNES: a function passed to nmiSet runs in interrupt context.
        if let ExprKind::Global(g) = &fptr.kind {
            if self.globals[g.0 as usize].name == "nmiSet" {
                if let Some(a) = out.first() {
                    if let Some(name) = self.function_named_by(a) {
                        self.interrupt_roots.insert(name);
                    }
                }
            }
        }
        mk(ExprKind::Call(Box::new(fptr), out), sig.ret, loc)
    }

    fn function_named_by(&self, e: &hir::Expr) -> Option<String> {
        match &e.kind {
            ExprKind::AddrOf(inner) | ExprKind::Cast(inner) => self.function_named_by(inner),
            ExprKind::Global(g) if self.globals[g.0 as usize].is_func => Some(self.globals[g.0 as usize].name.clone()),
            _ => None,
        }
    }
}

fn has_const_member(types: &Types, r: &Record) -> bool {
    r.fields.iter().any(|f| types.is_const(f.ty) || types.record(f.ty).map_or(false, |r2| has_const_member(types, r2)))
}

pub(crate) fn mask(bits: u32) -> u64 {
    if bits >= 64 {
        u64::MAX
    } else {
        (1u64 << bits) - 1
    }
}

fn parse_hex_float(s: &str) -> Option<f64> {
    let s = &s[2..];
    let (mant, exp) = match s.find(['p', 'P']) {
        Some(i) => (&s[..i], s[i + 1..].parse::<i32>().ok()?),
        None => (s, 0),
    };
    let (int, frac) = match mant.find('.') {
        Some(i) => (&mant[..i], &mant[i + 1..]),
        None => (mant, ""),
    };
    let mut v = 0f64;
    for c in int.chars() {
        v = v * 16.0 + c.to_digit(16)? as f64;
    }
    let mut scale = 1.0 / 16.0;
    for c in frac.chars() {
        v += c.to_digit(16)? as f64 * scale;
        scale /= 16.0;
    }
    Some(v * 2f64.powi(exp))
}
