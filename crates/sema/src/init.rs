//! Initialisers (static and automatic) and constant evaluation.

use crate::check::{section_for, Checker};
use crate::hir::{self, ExprKind, GlobalId, LocalId, StmtKind};
use crate::types::*;
use loomcc_parse::ast;
use loomcc_pp::Loc;

/// One scalar (or whole-struct) element of an initialiser, at a byte offset.
pub(crate) struct InitElem {
    pub offset: u64,
    pub ty: Ty,
    /// Bit-field placement: (bit, width, unit size).
    pub bits: Option<(u32, u32, u32)>,
    pub value: InitValue,
}

pub(crate) enum InitValue {
    Expr(hir::Expr),
    /// A string literal initialising a char array (bytes, element size).
    Bytes(Vec<u8>),
}

/// A constant value: integer, float, or an address (global + byte offset).
#[derive(Clone, Debug, PartialEq)]
pub enum ConstVal {
    Int(i64),
    Float(f64),
    Addr(GlobalId, i64),
}

impl Checker {
    // ------------------------------------------------------------------ constant evaluation

    /// Integer constant expression value.
    pub fn eval_int(&self, e: &hir::Expr) -> Option<i64> {
        match self.eval_const(e)? {
            ConstVal::Int(v) => Some(v),
            _ => None,
        }
    }

    pub fn eval_const(&self, e: &hir::Expr) -> Option<ConstVal> {
        use ConstVal::*;
        let ty = e.ty;
        let wrap = |v: i64| -> i64 {
            if self.types.is_integer(ty) {
                if matches!(self.types.kind(ty), TyKind::Bool) {
                    (v != 0) as i64
                } else {
                    self.wrap_to(v, ty)
                }
            } else {
                v
            }
        };
        match &e.kind {
            ExprKind::IntConst(v) => Some(Int(*v)),
            ExprKind::FloatConst(v) => Some(Float(*v)),
            ExprKind::Cast(inner) => {
                let v = self.eval_const(inner)?;
                if self.types.is_void(ty) {
                    return None;
                }
                match v {
                    Int(x) => {
                        if self.types.is_float(ty) {
                            if self.types.is_signed(inner.ty) || !self.types.is_integer(inner.ty) {
                                Some(Float(x as f64))
                            } else {
                                Some(Float(x as u64 as f64))
                            }
                        } else {
                            // Unsigned source: zero-extend before rewrapping.
                            let x = if self.types.is_integer(inner.ty) && !self.types.is_signed(inner.ty) {
                                (x as u64 & crate::expr::mask(self.types.bits(inner.ty))) as i64
                            } else {
                                x
                            };
                            Some(Int(wrap(x)))
                        }
                    }
                    Float(f) => {
                        if self.types.is_float(ty) {
                            Some(Float(f))
                        } else if matches!(self.types.kind(ty), TyKind::Bool) {
                            Some(Int((f != 0.0) as i64))
                        } else {
                            Some(Int(wrap(f as i64)))
                        }
                    }
                    Addr(g, o) => {
                        if self.types.is_ptr(ty) || self.types.size(ty) >= 2 {
                            Some(Addr(g, o))
                        } else {
                            None
                        }
                    }
                }
            }
            ExprKind::Unary(op, a) => {
                let v = self.eval_const(a)?;
                match (op, v) {
                    (hir::UnOp::Neg, Int(x)) => Some(Int(wrap(x.wrapping_neg()))),
                    (hir::UnOp::Neg, Float(x)) => Some(Float(-x)),
                    (hir::UnOp::BitNot, Int(x)) => Some(Int(wrap(!x))),
                    (hir::UnOp::Not, Int(x)) => Some(Int((x == 0) as i64)),
                    (hir::UnOp::Not, Float(x)) => Some(Int((x == 0.0) as i64)),
                    (hir::UnOp::Not, Addr(..)) => Some(Int(0)),
                    _ => None,
                }
            }
            ExprKind::Binary(op, a, b) => {
                let (x, y) = (self.eval_const(a)?, self.eval_const(b)?);
                match (x, y) {
                    (Int(p), Int(q)) => {
                        let signed = self.types.is_signed(ty);
                        let bits = self.types.bits(ty);
                        let m = crate::expr::mask(bits);
                        let (up, uq) = ((p as u64) & m, (q as u64) & m);
                        let v = match op {
                            hir::BinOp::Add => p.wrapping_add(q),
                            hir::BinOp::Sub => p.wrapping_sub(q),
                            hir::BinOp::Mul => p.wrapping_mul(q),
                            hir::BinOp::Div => {
                                if q == 0 {
                                    return None;
                                }
                                if signed { p.wrapping_div(q) } else { (up / uq) as i64 }
                            }
                            hir::BinOp::Rem => {
                                if q == 0 {
                                    return None;
                                }
                                if signed { p.wrapping_rem(q) } else { (up % uq) as i64 }
                            }
                            hir::BinOp::And => p & q,
                            hir::BinOp::Or => p | q,
                            hir::BinOp::Xor => p ^ q,
                            hir::BinOp::Shl => {
                                if q < 0 || q >= 64 {
                                    return None;
                                }
                                p.wrapping_shl(q as u32)
                            }
                            hir::BinOp::Shr => {
                                if q < 0 || q >= 64 {
                                    return None;
                                }
                                if signed { p >> q } else { (up >> q) as i64 }
                            }
                        };
                        Some(Int(wrap(v)))
                    }
                    (Float(p), Float(q)) => Some(Float(match op {
                        hir::BinOp::Add => p + q,
                        hir::BinOp::Sub => p - q,
                        hir::BinOp::Mul => p * q,
                        hir::BinOp::Div => p / q,
                        _ => return None,
                    })),
                    _ => None,
                }
            }
            ExprKind::Cmp(op, a, b) => {
                let (x, y) = (self.eval_const(a)?, self.eval_const(b)?);
                let r = match (x, y) {
                    (Int(p), Int(q)) => {
                        let signed = self.types.is_signed(a.ty);
                        let bits = self.types.bits(a.ty);
                        let m = crate::expr::mask(bits);
                        let (p, q) = if signed { (p as i128, q as i128) } else { ((p as u64 & m) as i128, (q as u64 & m) as i128) };
                        cmp(*op, p.cmp(&q))
                    }
                    (Float(p), Float(q)) => cmp(*op, p.partial_cmp(&q)?),
                    (Addr(g1, o1), Addr(g2, o2)) if g1 == g2 => cmp(*op, o1.cmp(&o2)),
                    (Addr(..), Int(0)) | (Int(0), Addr(..)) => match op {
                        hir::CmpOp::Eq => false,
                        hir::CmpOp::Ne => true,
                        _ => return None,
                    },
                    _ => return None,
                };
                Some(Int(r as i64))
            }
            ExprKind::LogAnd(a, b) => {
                let x = self.truth(a)?;
                if !x {
                    return Some(Int(0));
                }
                Some(Int(self.truth(b)? as i64))
            }
            ExprKind::LogOr(a, b) => {
                let x = self.truth(a)?;
                if x {
                    return Some(Int(1));
                }
                Some(Int(self.truth(b)? as i64))
            }
            ExprKind::Cond(c, a, b) => {
                if self.truth(c)? {
                    self.eval_const(a)
                } else {
                    self.eval_const(b)
                }
            }
            ExprKind::Comma(_, _) => None,
            ExprKind::AddrOf(inner) => self.eval_addr(inner).map(|(g, o)| Addr(g, o)),
            ExprKind::PtrAdd(p, i, scale) => {
                let pv = self.eval_const(p)?;
                let iv = self.eval_int(i)?;
                match pv {
                    Addr(g, o) => Some(Addr(g, o + iv * *scale as i64)),
                    Int(x) => Some(Int(x + iv * *scale as i64)),
                    _ => None,
                }
            }
            ExprKind::PtrDiff(a, b, scale) => match (self.eval_const(a)?, self.eval_const(b)?) {
                (Addr(g1, o1), Addr(g2, o2)) if g1 == g2 => Some(Int((o1 - o2) / *scale as i64)),
                (Int(x), Int(y)) => Some(Int((x - y) / *scale as i64)),
                _ => None,
            },
            _ => None,
        }
    }

    fn truth(&self, e: &hir::Expr) -> Option<bool> {
        match self.eval_const(e)? {
            ConstVal::Int(v) => Some(v != 0),
            ConstVal::Float(f) => Some(f != 0.0),
            ConstVal::Addr(..) => Some(true),
        }
    }

    /// The address of a constant lvalue: (global, byte offset).
    fn eval_addr(&self, e: &hir::Expr) -> Option<(GlobalId, i64)> {
        match &e.kind {
            ExprKind::Global(g) => Some((*g, 0)),
            ExprKind::Str(g) => Some((*g, 0)),
            ExprKind::Member(inner, off) => self.eval_addr(inner).map(|(g, o)| (g, o + *off as i64)),
            ExprKind::Deref(p) => match self.eval_const(p)? {
                ConstVal::Addr(g, o) => Some((g, o)),
                _ => None,
            },
            _ => None,
        }
    }

    // ------------------------------------------------------------------ initialiser traversal

    pub(crate) fn array_len_from_init(&mut self, elem: Ty, init: &ast::Initializer) -> u64 {
        match init {
            ast::Initializer::Expr(e) => {
                if let ast::ExprKind::Str(parts) = &e.kind {
                    if self.types.is_integer(elem) {
                        let s = self.string_literal(parts, e.loc);
                        if let TyKind::Array(_, Some(n)) = self.types.kind(s.ty) {
                            return *n;
                        }
                    }
                }
                1
            }
            ast::Initializer::List(items, _) => {
                // A single string in braces.
                if items.len() == 1 && items[0].designators.is_empty() {
                    if let ast::Initializer::Expr(e) = &items[0].init {
                        if let ast::ExprKind::Str(parts) = &e.kind {
                            if self.types.is_integer(elem) && self.types.size(elem) <= 4 {
                                let s = self.string_literal(parts, e.loc);
                                if let TyKind::Array(_, Some(n)) = self.types.kind(s.ty) {
                                    return *n;
                                }
                            }
                        }
                    }
                }
                let mut max = 0u64;
                let mut idx = 0u64;
                let mut i = 0;
                while i < items.len() {
                    let it = &items[i];
                    if let Some(first) = it.designators.first() {
                        match first {
                            ast::Designator::Index(e) => {
                                let x = self.expr(e);
                                idx = self.eval_int(&x).unwrap_or(0).max(0) as u64;
                            }
                            ast::Designator::Range(a, b) => {
                                let xa = self.expr(a);
                                let _ = xa;
                                let xb = self.expr(b);
                                idx = self.eval_int(&xb).unwrap_or(0).max(0) as u64;
                            }
                            ast::Designator::Field(..) => {}
                        }
                        i += 1;
                    } else {
                        // Count how many items this element consumes (brace
                        // elision for aggregate elements).
                        let consumed = self.items_for(elem, &items[i..]);
                        i += consumed.max(1);
                    }
                    idx += 1;
                    max = max.max(idx);
                }
                max
            }
        }
    }

    /// How many consecutive undesignated items initialise one object of type
    /// `ty` under brace elision.
    fn items_for(&mut self, ty: Ty, items: &[ast::InitItem]) -> usize {
        if items.is_empty() {
            return 0;
        }
        if matches!(items[0].init, ast::Initializer::List(..)) || !self.is_aggregate(ty) {
            return 1;
        }
        if let ast::Initializer::Expr(e) = &items[0].init {
            if let ast::ExprKind::Str(_) = &e.kind {
                if let Some(el) = self.types.elem(ty) {
                    if self.types.is_integer(el) {
                        return 1;
                    }
                }
            }
        }
        match self.types.kind(ty).clone() {
            TyKind::Array(e, Some(n)) => {
                let mut used = 0;
                for _ in 0..n {
                    if used >= items.len() || !items[used].designators.is_empty() {
                        break;
                    }
                    used += self.items_for(e, &items[used..]).max(1);
                }
                used
            }
            TyKind::Record(_) => {
                let r = self.types.record(ty).unwrap().clone();
                let fields: Vec<Ty> = if r.is_union { r.fields.iter().take(1).map(|f| f.ty).collect() } else { r.fields.iter().map(|f| f.ty).collect() };
                let mut used = 0;
                for f in fields {
                    if used >= items.len() || !items[used].designators.is_empty() {
                        break;
                    }
                    used += self.items_for(f, &items[used..]).max(1);
                }
                used
            }
            _ => 1,
        }
    }

    fn is_aggregate(&self, ty: Ty) -> bool {
        matches!(self.types.kind(ty), TyKind::Array(..) | TyKind::Record(_))
    }

    /// Collects the elements initialising an object of type `ty`.
    pub(crate) fn collect_init(&mut self, ty: Ty, init: &ast::Initializer, offset: u64, out: &mut Vec<InitElem>) {
        match init {
            ast::Initializer::Expr(e) => {
                // A string literal for a char array.
                if let (Some(el), ast::ExprKind::Str(parts)) = (self.types.elem(ty), &e.kind) {
                    if self.types.is_integer(el) {
                        let s = self.string_literal(parts, e.loc);
                        self.push_string(ty, &s, offset, out);
                        return;
                    }
                }
                let x = self.expr(e);
                let x = self.rvalue(x);
                if self.is_aggregate(ty) {
                    if self.types.is_record(ty) && self.types.compatible_unqual(ty, x.ty) {
                        out.push(InitElem { offset, ty, bits: None, value: InitValue::Expr(x) });
                        return;
                    }
                    // Brace elision for a lone scalar: initialise the first
                    // scalar sub-object.
                    let items = vec![ast::InitItem { designators: vec![], init: init.clone() }];
                    let mut idx = 0;
                    self.list_into(ty, &items, &mut idx, offset, out, true);
                    return;
                }
                let x = self.assign_convert(x, ty, e.loc, "initialization");
                out.push(InitElem { offset, ty, bits: None, value: InitValue::Expr(x) });
            }
            ast::Initializer::List(items, loc) => {
                if !self.is_aggregate(ty) {
                    // Scalar in braces.
                    match items.first() {
                        Some(first) => {
                            if items.len() > 1 {
                                self.warn(*loc, "excess elements in scalar initializer");
                            }
                            self.collect_init(ty, &first.init, offset, out);
                        }
                        None => {
                            let z = hir::Expr { kind: ExprKind::IntConst(0), ty: Types::INT, loc: *loc };
                            let z = self.convert(z, ty);
                            out.push(InitElem { offset, ty, bits: None, value: InitValue::Expr(z) });
                        }
                    }
                    return;
                }
                // `char s[] = {"abc"}`
                if items.len() == 1 && items[0].designators.is_empty() {
                    if let (Some(el), ast::Initializer::Expr(e)) = (self.types.elem(ty), &items[0].init) {
                        if let ast::ExprKind::Str(parts) = &e.kind {
                            if self.types.is_integer(el) {
                                let s = self.string_literal(parts, e.loc);
                                self.push_string(ty, &s, offset, out);
                                return;
                            }
                        }
                    }
                }
                let mut idx = 0;
                self.list_into(ty, items, &mut idx, offset, out, true);
                if idx < items.len() {
                    self.warn(*loc, "excess elements in initializer");
                }
            }
        }
    }

    fn push_string(&mut self, ty: Ty, s: &hir::Expr, offset: u64, out: &mut Vec<InitElem>) {
        let ExprKind::Str(g) = s.kind else { return };
        let mut bytes = self.globals[g.0 as usize].init.as_ref().unwrap().bytes.clone();
        let n = self.types.size(ty) as usize;
        if bytes.len() > n {
            // `char s[3] = "abc"` drops the NUL; longer is an error.
            if bytes.len() > n + self.types.size(self.types.elem(ty).unwrap()) as usize {
                self.warn(s.loc, "initializer-string for char array is too long");
            }
            bytes.truncate(n);
        }
        out.push(InitElem { offset, ty, bits: None, value: InitValue::Bytes(bytes) });
    }

    /// Initialises the aggregate `ty` from `items[*idx..]`. `braced`: this
    /// list is the aggregate's own brace list (designators apply here);
    /// otherwise it is brace elision and stops at a designator.
    fn list_into(&mut self, ty: Ty, items: &[ast::InitItem], idx: &mut usize, offset: u64, out: &mut Vec<InitElem>, braced: bool) {
        match self.types.kind(ty).clone() {
            TyKind::Array(elem, n) => {
                let esize = self.types.size(elem);
                let mut pos: u64 = 0;
                while *idx < items.len() {
                    let it = &items[*idx];
                    if !it.designators.is_empty() {
                        if !braced {
                            return;
                        }
                        let (lo, hi) = match &it.designators[0] {
                            ast::Designator::Index(e) => {
                                let x = self.expr(e);
                                let v = self.eval_int(&x).unwrap_or(0).max(0) as u64;
                                (v, v)
                            }
                            ast::Designator::Range(a, b) => {
                                let xa = self.expr(a);
                                let xb = self.expr(b);
                                (self.eval_int(&xa).unwrap_or(0).max(0) as u64, self.eval_int(&xb).unwrap_or(0).max(0) as u64)
                            }
                            ast::Designator::Field(_, l) => {
                                self.error(*l, "field designator in an array initializer");
                                *idx += 1;
                                continue;
                            }
                        };
                        if n.map_or(false, |n| hi >= n) {
                            self.error(items[*idx].init_loc(), "array designator index exceeds array bounds");
                            *idx += 1;
                            continue;
                        }
                        for p in lo..=hi {
                            self.designated(elem, &it.designators[1..], &it.init, offset + p * esize, out);
                        }
                        *idx += 1;
                        pos = hi + 1;
                        continue;
                    }
                    if n.map_or(false, |n| pos >= n) {
                        return;
                    }
                    self.sub_init(elem, items, idx, offset + pos * esize, out);
                    pos += 1;
                }
            }
            TyKind::Record(_) => {
                let r = self.types.record(ty).unwrap().clone();
                let mut fi = 0usize;
                while *idx < items.len() {
                    let it = &items[*idx];
                    if !it.designators.is_empty() {
                        if !braced {
                            return;
                        }
                        let ast::Designator::Field(name, l) = &it.designators[0] else {
                            self.error(it.init_loc(), "array designator in a struct initializer");
                            *idx += 1;
                            continue;
                        };
                        // Find the field (possibly inside an anonymous member).
                        match r.fields.iter().position(|f| f.name.as_deref() == Some(name.as_str())) {
                            Some(p) => {
                                let f = r.fields[p].clone();
                                if f.bits.is_some() {
                                    self.bitfield_init(&f, &it.init, offset, out);
                                } else {
                                    self.designated(f.ty, &it.designators[1..], &it.init, offset + f.offset, out);
                                }
                                *idx += 1;
                                fi = p + 1;
                            }
                            None => {
                                // Anonymous member containing it?
                                let anon = r.fields.iter().position(|f| f.name.is_none() && self.types.find_field(f.ty, name).is_some());
                                match anon {
                                    Some(p) => {
                                        let f = r.fields[p].clone();
                                        self.list_into(f.ty, items, idx, offset + f.offset, out, true);
                                        fi = p + 1;
                                    }
                                    None => {
                                        self.error(*l, format!("field designator '{}' does not refer to any field", name));
                                        *idx += 1;
                                    }
                                }
                            }
                        }
                        if r.is_union {
                            return;
                        }
                        continue;
                    }
                    // Skip unnamed bit-field padding.
                    while fi < r.fields.len() && r.fields[fi].name.is_none() && r.fields[fi].bits.is_some() {
                        fi += 1;
                    }
                    if fi >= r.fields.len() || (r.is_union && fi > 0) {
                        return;
                    }
                    let f = r.fields[fi].clone();
                    if f.bits.is_some() {
                        let init = items[*idx].init.clone();
                        self.bitfield_init(&f, &init, offset, out);
                        *idx += 1;
                    } else {
                        self.sub_init(f.ty, items, idx, offset + f.offset, out);
                    }
                    fi += 1;
                }
            }
            _ => {
                // Scalar reached through elision.
                if *idx < items.len() {
                    let init = items[*idx].init.clone();
                    self.collect_init(ty, &init, offset, out);
                    *idx += 1;
                }
            }
        }
    }

    fn bitfield_init(&mut self, f: &Field, init: &ast::Initializer, offset: u64, out: &mut Vec<InitElem>) {
        let mut tmp = Vec::new();
        self.collect_init(f.ty, init, 0, &mut tmp);
        for mut e in tmp {
            e.offset = offset + f.offset;
            e.bits = f.bits;
            out.push(e);
        }
    }

    /// One sub-object from the item stream (braced, string, whole struct, or
    /// elided).
    fn sub_init(&mut self, ty: Ty, items: &[ast::InitItem], idx: &mut usize, offset: u64, out: &mut Vec<InitElem>) {
        let it = &items[*idx];
        match &it.init {
            ast::Initializer::List(..) => {
                let init = it.init.clone();
                self.collect_init(ty, &init, offset, out);
                *idx += 1;
            }
            ast::Initializer::Expr(e) => {
                if !self.is_aggregate(ty) {
                    let init = it.init.clone();
                    self.collect_init(ty, &init, offset, out);
                    *idx += 1;
                    return;
                }
                // String for a char array, or a struct value.
                if let (Some(el), ast::ExprKind::Str(_)) = (self.types.elem(ty), &e.kind) {
                    if self.types.is_integer(el) {
                        let init = it.init.clone();
                        self.collect_init(ty, &init, offset, out);
                        *idx += 1;
                        return;
                    }
                }
                if self.types.is_record(ty) {
                    // Peek at the expression's type without side effects on
                    // diagnostics: check it once.
                    let x = self.expr(e);
                    if self.types.compatible_unqual(ty, x.ty) {
                        let x = self.rvalue(x);
                        out.push(InitElem { offset, ty, bits: None, value: InitValue::Expr(x) });
                        *idx += 1;
                        return;
                    }
                }
                self.list_into(ty, items, idx, offset, out, false);
            }
        }
    }

    fn designated(&mut self, ty: Ty, rest: &[ast::Designator], init: &ast::Initializer, offset: u64, out: &mut Vec<InitElem>) {
        if rest.is_empty() {
            match init {
                ast::Initializer::Expr(_) if self.is_aggregate(ty) => {
                    let items = vec![ast::InitItem { designators: vec![], init: init.clone() }];
                    let mut idx = 0;
                    self.sub_init(ty, &items, &mut idx, offset, out);
                }
                _ => self.collect_init(ty, init, offset, out),
            }
            return;
        }
        match &rest[0] {
            ast::Designator::Field(name, l) => match self.types.find_field(ty, name) {
                Some((off, f)) => {
                    if f.bits.is_some() {
                        self.bitfield_init(&f, init, offset + off - f.offset, out);
                    } else {
                        self.designated(f.ty, &rest[1..], init, offset + off, out);
                    }
                }
                None => self.error(*l, format!("field designator '{}' does not refer to any field", name)),
            },
            ast::Designator::Index(e) => {
                let x = self.expr(e);
                let i = self.eval_int(&x).unwrap_or(0).max(0) as u64;
                let el = self.types.elem(ty).unwrap_or(Types::INT);
                let es = self.types.size(el);
                self.designated(el, &rest[1..], init, offset + i * es, out);
            }
            ast::Designator::Range(a, b) => {
                let xa = self.expr(a);
                let xb = self.expr(b);
                let (lo, hi) = (self.eval_int(&xa).unwrap_or(0).max(0) as u64, self.eval_int(&xb).unwrap_or(0).max(0) as u64);
                let el = self.types.elem(ty).unwrap_or(Types::INT);
                let es = self.types.size(el);
                for i in lo..=hi {
                    self.designated(el, &rest[1..], init, offset + i * es, out);
                }
            }
        }
    }

    // ------------------------------------------------------------------ static initialisers

    pub(crate) fn static_initializer(&mut self, gid: GlobalId, init: &ast::Initializer) {
        let mut ty = self.globals[gid.0 as usize].ty;
        if let TyKind::Array(e, None) = self.types.kind(ty).clone() {
            let n = self.array_len_from_init(e, init);
            let q = self.types.quals(ty);
            ty = self.types.array(e, Some(n));
            ty = self.types.with_quals(ty, q);
            self.globals[gid.0 as usize].ty = ty;
        }
        if !self.types.is_complete(ty) {
            let loc = self.globals[gid.0 as usize].loc;
            self.error(loc, format!("variable has incomplete type '{}'", self.types.display(ty)));
            return;
        }
        let mut elems = Vec::new();
        self.collect_init(ty, init, 0, &mut elems);
        let size = self.types.size(ty) as usize;
        let mut si = hir::StaticInit { bytes: vec![0; size], relocs: vec![] };
        for el in elems {
            self.write_static(&mut si, &el);
        }
        let nonzero = si.bytes.iter().any(|&b| b != 0) || !si.relocs.is_empty();
        let section = section_for(&self.types, ty, nonzero);
        let g = &mut self.globals[gid.0 as usize];
        g.init = Some(si);
        g.section = section;
        g.defined = true;
    }

    fn write_static(&mut self, si: &mut hir::StaticInit, el: &InitElem) {
        let off = el.offset as usize;
        match &el.value {
            InitValue::Bytes(b) => {
                let end = (off + b.len()).min(si.bytes.len());
                si.bytes[off..end].copy_from_slice(&b[..end - off]);
            }
            InitValue::Expr(e) => {
                if self.types.is_record(el.ty) {
                    // A struct value: a constant object's bytes.
                    if let Some(bytes) = self.const_object_bytes(e) {
                        let end = (off + bytes.len()).min(si.bytes.len());
                        si.bytes[off..end].copy_from_slice(&bytes[..end - off]);
                        return;
                    }
                    self.error(e.loc, "initializer element is not a compile-time constant");
                    return;
                }
                let size = self.types.size(el.ty) as usize;
                match self.eval_const(e) {
                    Some(ConstVal::Int(v)) => {
                        if let Some((bit, width, unit)) = el.bits {
                            let unit = unit as usize;
                            let mut word: u64 = 0;
                            for i in 0..unit {
                                word |= (si.bytes[off + i] as u64) << (8 * i);
                            }
                            let m = crate::expr::mask(width) << bit;
                            word = (word & !m) | (((v as u64) << bit) & m);
                            for i in 0..unit {
                                si.bytes[off + i] = (word >> (8 * i)) as u8;
                            }
                        } else {
                            for i in 0..size {
                                si.bytes[off + i] = ((v as u64) >> (8 * i.min(7))) as u8;
                                if i >= 8 {
                                    si.bytes[off + i] = if v < 0 { 0xff } else { 0 };
                                }
                            }
                        }
                    }
                    Some(ConstVal::Float(f)) => {
                        let bytes: Vec<u8> = if size == 4 { (f as f32).to_le_bytes().to_vec() } else { f.to_le_bytes().to_vec() };
                        si.bytes[off..off + size].copy_from_slice(&bytes[..size]);
                    }
                    Some(ConstVal::Addr(g, a)) => {
                        let width = if size >= 4 { 4 } else { 2 };
                        si.relocs.push(hir::Reloc { offset: el.offset, target: g, addend: a, width });
                        self.globals[g.0 as usize].address_taken = true;
                    }
                    None => self.error(e.loc, "initializer element is not a compile-time constant"),
                }
            }
        }
    }

    /// Bytes of a constant struct-valued expression (a compound literal or a
    /// const global with an initialiser).
    fn const_object_bytes(&self, e: &hir::Expr) -> Option<Vec<u8>> {
        match &e.kind {
            ExprKind::Global(g) => {
                let gl = &self.globals[g.0 as usize];
                if self.types.is_const(gl.ty) || gl.name.starts_with(".compound.") {
                    gl.init.as_ref().filter(|i| i.relocs.is_empty()).map(|i| i.bytes.clone())
                } else {
                    None
                }
            }
            _ => None,
        }
    }

    // ------------------------------------------------------------------ automatic initialisers

    pub(crate) fn local_initializer(&mut self, lid: LocalId, ty: Ty, init: &ast::Initializer, loc: Loc) -> Vec<hir::Stmt> {
        let mut elems = Vec::new();
        self.collect_init(ty, init, 0, &mut elems);
        let target = hir::Expr { kind: ExprKind::Local(lid), ty, loc };
        let mut out = Vec::new();
        let scalar_whole = elems.len() == 1 && elems[0].offset == 0 && elems[0].bits.is_none() && self.types.unqual(elems[0].ty) == self.types.unqual(ty);
        if !scalar_whole {
            // Aggregates: zero the whole object, then store the elements.
            out.push(hir::Stmt { kind: StmtKind::Expr(hir::Expr { kind: ExprKind::ZeroInit(Box::new(target.clone())), ty: Types::VOID, loc }), loc });
        }
        for el in elems {
            match el.value {
                InitValue::Bytes(bytes) => {
                    // Store each element of the string.
                    let elem_ty = self.types.elem(el.ty).unwrap_or(Types::CHAR);
                    let es = self.types.size(elem_ty) as usize;
                    for (i, chunk) in bytes.chunks(es).enumerate() {
                        let mut v: i64 = 0;
                        for (k, b) in chunk.iter().enumerate() {
                            v |= (*b as i64) << (8 * k);
                        }
                        if v == 0 {
                            continue;
                        }
                        let lv = self.sub_lvalue(&target, el.offset + (i * es) as u64, elem_ty, loc);
                        let val = hir::Expr { kind: ExprKind::IntConst(self.wrap_to(v, elem_ty)), ty: self.types.unqual(elem_ty), loc };
                        let t = lv.ty;
                        out.push(hir::Stmt { kind: StmtKind::Expr(hir::Expr { kind: ExprKind::Assign(Box::new(lv), Box::new(val)), ty: t, loc }), loc });
                    }
                }
                InitValue::Expr(e) => {
                    let lv = match el.bits {
                        Some((bit, width, unit)) => {
                            let base = self.sub_lvalue(&target, el.offset, Types::UCHAR, loc);
                            let obj = match base.kind {
                                ExprKind::Member(o, off) => (o, off),
                                _ => (Box::new(target.clone()), el.offset),
                            };
                            let signed = self.types.is_signed(el.ty);
                            hir::Expr { kind: ExprKind::BitField(obj.0, obj.1, bit, width, unit, signed), ty: el.ty, loc }
                        }
                        None => self.sub_lvalue(&target, el.offset, el.ty, loc),
                    };
                    if scalar_whole {
                        let t = self.types.unqual(ty);
                        out.push(hir::Stmt { kind: StmtKind::Expr(hir::Expr { kind: ExprKind::Assign(Box::new(target.clone()), Box::new(e)), ty: t, loc }), loc });
                        continue;
                    }
                    let t = self.types.unqual(lv.ty);
                    out.push(hir::Stmt { kind: StmtKind::Expr(hir::Expr { kind: ExprKind::Assign(Box::new(lv), Box::new(e)), ty: t, loc }), loc });
                }
            }
        }
        out
    }

    /// An lvalue for the sub-object at `offset` of type `ty` inside `base`.
    fn sub_lvalue(&mut self, base: &hir::Expr, offset: u64, ty: Ty, loc: Loc) -> hir::Expr {
        hir::Expr { kind: ExprKind::Member(Box::new(base.clone()), offset), ty, loc }
    }
}

fn cmp(op: hir::CmpOp, o: std::cmp::Ordering) -> bool {
    use std::cmp::Ordering::*;
    match op {
        hir::CmpOp::Eq => o == Equal,
        hir::CmpOp::Ne => o != Equal,
        hir::CmpOp::Lt => o == Less,
        hir::CmpOp::Le => o != Greater,
        hir::CmpOp::Gt => o == Greater,
        hir::CmpOp::Ge => o != Less,
    }
}

trait InitLoc {
    fn init_loc(&self) -> Loc;
}

impl InitLoc for ast::InitItem {
    fn init_loc(&self) -> Loc {
        match &self.init {
            ast::Initializer::Expr(e) => e.loc,
            ast::Initializer::List(_, l) => *l,
        }
    }
}
