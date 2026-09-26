//! Types, interned, and their 65816 layout (816-tcc compatible).

use std::collections::HashMap;
use std::fmt::Write as _;

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug, PartialOrd, Ord)]
pub struct Ty(pub u32);

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
pub enum IntKind {
    Char,
    SChar,
    UChar,
    Short,
    UShort,
    Int,
    UInt,
    Long,
    ULong,
    LongLong,
    ULongLong,
}

impl IntKind {
    pub fn is_signed(self) -> bool {
        matches!(self, IntKind::Char | IntKind::SChar | IntKind::Short | IntKind::Int | IntKind::Long | IntKind::LongLong)
    }

    /// Conversion rank (C17 6.3.1.1).
    pub fn rank(self) -> u8 {
        match self {
            IntKind::Char | IntKind::SChar | IntKind::UChar => 1,
            IntKind::Short | IntKind::UShort => 2,
            IntKind::Int | IntKind::UInt => 3,
            IntKind::Long | IntKind::ULong => 4,
            IntKind::LongLong | IntKind::ULongLong => 5,
        }
    }

    pub fn to_unsigned(self) -> IntKind {
        match self {
            IntKind::Char | IntKind::SChar | IntKind::UChar => IntKind::UChar,
            IntKind::Short | IntKind::UShort => IntKind::UShort,
            IntKind::Int | IntKind::UInt => IntKind::UInt,
            IntKind::Long | IntKind::ULong => IntKind::ULong,
            IntKind::LongLong | IntKind::ULongLong => IntKind::ULongLong,
        }
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
pub enum FloatKind {
    Float,
    Double,
    LongDouble,
}

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
pub struct RecordId(pub u32);

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
pub struct EnumId(pub u32);

#[derive(Clone, PartialEq, Eq, Hash, Debug)]
pub enum TyKind {
    Void,
    Bool,
    Int(IntKind),
    Float(FloatKind),
    Ptr(Ty),
    Array(Ty, Option<u64>),
    Func(FuncSig),
    Record(RecordId),
    Enum(EnumId),
}

#[derive(Clone, PartialEq, Eq, Hash, Debug)]
pub struct FuncSig {
    pub ret: Ty,
    pub params: Vec<Ty>,
    pub variadic: bool,
    /// Declared with a prototype (`f(void)`, `f(int)`), not `f()`.
    pub proto: bool,
}

pub const Q_CONST: u8 = 1;
pub const Q_VOLATILE: u8 = 2;
pub const Q_RESTRICT: u8 = 4;

#[derive(Clone, PartialEq, Eq, Hash, Debug)]
struct TyData {
    kind: TyKind,
    quals: u8,
}

#[derive(Clone, Debug)]
pub struct Field {
    pub name: Option<String>,
    pub ty: Ty,
    pub offset: u64,
    /// Bit-field: (bit offset within the storage unit at `offset`, width,
    /// storage unit size in bytes).
    pub bits: Option<(u32, u32, u32)>,
}

#[derive(Clone, Debug)]
pub struct Record {
    pub is_union: bool,
    pub name: Option<String>,
    pub complete: bool,
    pub fields: Vec<Field>,
    pub size: u64,
    pub align: u64,
}

#[derive(Clone, Debug)]
pub struct EnumInfo {
    pub name: Option<String>,
    pub complete: bool,
}

/// Target data model. `Layout::snes()` is 816-tcc's (pointers four bytes,
/// aligned to four inside structs) with a 32-bit `long`.
#[derive(Clone, Debug)]
pub struct Layout {
    pub ptr_size: u64,
    pub ptr_align: u64,
    pub long_size: u64,
    pub long_align: u64,
    pub llong_align: u64,
}

impl Layout {
    pub fn snes() -> Layout {
        Layout { ptr_size: 4, ptr_align: 4, long_size: 4, long_align: 2, llong_align: 2 }
    }

    /// Natural 65816 layout (pointers aligned to two) — for experiments on
    /// structs no assembly reads.
    pub fn packed_pointers() -> Layout {
        Layout { ptr_align: 2, ..Layout::snes() }
    }
}

pub struct Types {
    data: Vec<TyData>,
    map: HashMap<TyData, Ty>,
    pub records: Vec<Record>,
    pub enums: Vec<EnumInfo>,
    pub layout: Layout,
}

impl Types {
    pub fn new(layout: Layout) -> Types {
        let mut t = Types { data: Vec::new(), map: HashMap::new(), records: Vec::new(), enums: Vec::new(), layout };
        // Fixed ids for the common types (see the constants below).
        for k in [
            TyKind::Void,
            TyKind::Bool,
            TyKind::Int(IntKind::Char),
            TyKind::Int(IntKind::SChar),
            TyKind::Int(IntKind::UChar),
            TyKind::Int(IntKind::Short),
            TyKind::Int(IntKind::UShort),
            TyKind::Int(IntKind::Int),
            TyKind::Int(IntKind::UInt),
            TyKind::Int(IntKind::Long),
            TyKind::Int(IntKind::ULong),
            TyKind::Int(IntKind::LongLong),
            TyKind::Int(IntKind::ULongLong),
            TyKind::Float(FloatKind::Float),
            TyKind::Float(FloatKind::Double),
            TyKind::Float(FloatKind::LongDouble),
        ] {
            t.intern(k, 0);
        }
        t
    }

    pub const VOID: Ty = Ty(0);
    pub const BOOL: Ty = Ty(1);
    pub const CHAR: Ty = Ty(2);
    pub const SCHAR: Ty = Ty(3);
    pub const UCHAR: Ty = Ty(4);
    pub const SHORT: Ty = Ty(5);
    pub const USHORT: Ty = Ty(6);
    pub const INT: Ty = Ty(7);
    pub const UINT: Ty = Ty(8);
    pub const LONG: Ty = Ty(9);
    pub const ULONG: Ty = Ty(10);
    pub const LLONG: Ty = Ty(11);
    pub const ULLONG: Ty = Ty(12);
    pub const FLOAT: Ty = Ty(13);
    pub const DOUBLE: Ty = Ty(14);
    pub const LDOUBLE: Ty = Ty(15);

    pub fn intern(&mut self, kind: TyKind, quals: u8) -> Ty {
        let d = TyData { kind, quals };
        if let Some(&t) = self.map.get(&d) {
            return t;
        }
        let t = Ty(self.data.len() as u32);
        self.data.push(d.clone());
        self.map.insert(d, t);
        t
    }

    pub fn kind(&self, t: Ty) -> &TyKind {
        &self.data[t.0 as usize].kind
    }

    pub fn quals(&self, t: Ty) -> u8 {
        self.data[t.0 as usize].quals
    }

    pub fn is_const(&self, t: Ty) -> bool {
        self.quals(t) & Q_CONST != 0
    }

    pub fn is_volatile(&self, t: Ty) -> bool {
        self.quals(t) & Q_VOLATILE != 0
    }

    pub fn unqual(&mut self, t: Ty) -> Ty {
        if self.quals(t) == 0 {
            return t;
        }
        let k = self.kind(t).clone();
        self.intern(k, 0)
    }

    pub fn with_quals(&mut self, t: Ty, quals: u8) -> Ty {
        if quals == 0 {
            return t;
        }
        // Qualifiers on an array type apply to its elements (C17 6.7.3p10).
        if let TyKind::Array(e, n) = self.kind(t).clone() {
            let e = self.with_quals(e, quals);
            let q = self.quals(t);
            return self.intern(TyKind::Array(e, n), q);
        }
        let k = self.kind(t).clone();
        let q = self.quals(t) | quals;
        self.intern(k, q)
    }

    pub fn ptr(&mut self, t: Ty) -> Ty {
        self.intern(TyKind::Ptr(t), 0)
    }

    pub fn array(&mut self, t: Ty, n: Option<u64>) -> Ty {
        self.intern(TyKind::Array(t, n), 0)
    }

    pub fn func(&mut self, sig: FuncSig) -> Ty {
        self.intern(TyKind::Func(sig), 0)
    }

    pub fn int_kind(&self, t: Ty) -> Option<IntKind> {
        match self.kind(t) {
            TyKind::Int(k) => Some(*k),
            TyKind::Bool => Some(IntKind::UChar),
            TyKind::Enum(_) => Some(IntKind::Int),
            _ => None,
        }
    }

    pub fn is_integer(&self, t: Ty) -> bool {
        matches!(self.kind(t), TyKind::Int(_) | TyKind::Bool | TyKind::Enum(_))
    }

    pub fn is_float(&self, t: Ty) -> bool {
        matches!(self.kind(t), TyKind::Float(_))
    }

    pub fn is_arith(&self, t: Ty) -> bool {
        self.is_integer(t) || self.is_float(t)
    }

    pub fn is_ptr(&self, t: Ty) -> bool {
        matches!(self.kind(t), TyKind::Ptr(_))
    }

    pub fn is_scalar(&self, t: Ty) -> bool {
        self.is_arith(t) || self.is_ptr(t)
    }

    pub fn is_void(&self, t: Ty) -> bool {
        matches!(self.kind(t), TyKind::Void)
    }

    pub fn is_array(&self, t: Ty) -> bool {
        matches!(self.kind(t), TyKind::Array(..))
    }

    pub fn is_func(&self, t: Ty) -> bool {
        matches!(self.kind(t), TyKind::Func(_))
    }

    pub fn is_record(&self, t: Ty) -> bool {
        matches!(self.kind(t), TyKind::Record(_))
    }

    pub fn is_signed(&self, t: Ty) -> bool {
        self.int_kind(t).map_or(false, |k| k.is_signed() && !matches!(self.kind(t), TyKind::Bool))
    }

    pub fn pointee(&self, t: Ty) -> Option<Ty> {
        match self.kind(t) {
            TyKind::Ptr(p) => Some(*p),
            _ => None,
        }
    }

    pub fn elem(&self, t: Ty) -> Option<Ty> {
        match self.kind(t) {
            TyKind::Array(e, _) => Some(*e),
            _ => None,
        }
    }

    pub fn sig(&self, t: Ty) -> Option<&FuncSig> {
        match self.kind(t) {
            TyKind::Func(s) => Some(s),
            _ => None,
        }
    }

    pub fn record(&self, t: Ty) -> Option<&Record> {
        match self.kind(t) {
            TyKind::Record(r) => Some(&self.records[r.0 as usize]),
            _ => None,
        }
    }

    pub fn is_complete(&self, t: Ty) -> bool {
        match self.kind(t) {
            TyKind::Void => false,
            TyKind::Array(e, n) => n.is_some() && self.is_complete(*e),
            TyKind::Record(r) => self.records[r.0 as usize].complete,
            TyKind::Enum(e) => self.enums[e.0 as usize].complete,
            TyKind::Func(_) => false,
            _ => true,
        }
    }

    pub fn size(&self, t: Ty) -> u64 {
        match self.kind(t) {
            TyKind::Void => 1,
            TyKind::Bool => 1,
            TyKind::Int(k) => match k {
                IntKind::Char | IntKind::SChar | IntKind::UChar => 1,
                IntKind::Short | IntKind::UShort | IntKind::Int | IntKind::UInt => 2,
                IntKind::Long | IntKind::ULong => self.layout.long_size,
                IntKind::LongLong | IntKind::ULongLong => 8,
            },
            TyKind::Float(FloatKind::Float) => 4,
            TyKind::Float(_) => 8,
            TyKind::Ptr(_) => self.layout.ptr_size,
            TyKind::Array(e, n) => self.size(*e) * n.unwrap_or(0),
            TyKind::Func(_) => 1,
            TyKind::Record(r) => self.records[r.0 as usize].size,
            TyKind::Enum(_) => 2,
        }
    }

    pub fn align(&self, t: Ty) -> u64 {
        match self.kind(t) {
            TyKind::Void | TyKind::Bool | TyKind::Func(_) => 1,
            TyKind::Int(k) => match k {
                IntKind::Char | IntKind::SChar | IntKind::UChar => 1,
                IntKind::Long | IntKind::ULong => self.layout.long_align,
                IntKind::LongLong | IntKind::ULongLong => self.layout.llong_align,
                _ => 2,
            },
            TyKind::Float(_) => 2,
            TyKind::Ptr(_) => self.layout.ptr_align,
            TyKind::Array(e, _) => self.align(*e),
            TyKind::Record(r) => self.records[r.0 as usize].align,
            TyKind::Enum(_) => 2,
        }
    }

    /// Integer width in bits (for integers, bool, enums).
    pub fn bits(&self, t: Ty) -> u32 {
        (self.size(t) * 8) as u32
    }

    /// Lays out a record's fields (816-tcc rules: fields at their natural
    /// alignment, bit-fields packed LSB-first into units of the declared
    /// type, a field that would straddle its unit starting the next one).
    pub fn layout_record(&mut self, id: RecordId, members: Vec<(Option<String>, Ty, Option<u32>)>) {
        let is_union = self.records[id.0 as usize].is_union;
        let mut fields = Vec::new();
        let mut offset: u64 = 0; // in bits
        let mut size_bits: u64 = 0;
        let mut align: u64 = 1;
        for (name, ty, width) in members {
            let fsize = self.size(ty);
            let falign = self.align(ty);
            match width {
                Some(w) => {
                    let unit_bits = fsize * 8;
                    if w == 0 {
                        // Zero width: pad to the next unit.
                        if !is_union {
                            offset = offset.div_ceil(unit_bits) * unit_bits;
                        }
                        continue;
                    }
                    let start = if is_union { 0 } else { offset };
                    let unit_start = (start / unit_bits) * unit_bits;
                    let (unit_start, bit) = if start + w as u64 > unit_start + unit_bits {
                        (unit_start + unit_bits, 0)
                    } else {
                        (unit_start, (start - unit_start) as u32)
                    };
                    // Bytes: the unit's byte offset must be aligned to the
                    // field type's alignment.
                    let byte = unit_start / 8;
                    fields.push(Field { name, ty, offset: byte, bits: Some((bit, w, fsize as u32)) });
                    let end = unit_start + bit as u64 + w as u64;
                    if !is_union {
                        offset = end;
                    }
                    size_bits = size_bits.max(end);
                    align = align.max(falign);
                }
                None => {
                    let start_bits = if is_union { 0 } else { offset };
                    let byte = start_bits.div_ceil(8).div_ceil(falign) * falign;
                    fields.push(Field { name, ty, offset: byte, bits: None });
                    let end = (byte + fsize) * 8;
                    if !is_union {
                        offset = end;
                    }
                    size_bits = size_bits.max(end);
                    align = align.max(falign);
                }
            }
        }
        let size = size_bits.div_ceil(8).div_ceil(align) * align;
        let r = &mut self.records[id.0 as usize];
        r.fields = fields;
        r.size = size;
        r.align = align;
        r.complete = true;
    }

    /// Finds a field by name, searching anonymous members. Returns the path of
    /// (offset accumulated, field).
    pub fn find_field(&self, rec: Ty, name: &str) -> Option<(u64, Field)> {
        let r = self.record(rec)?;
        for f in &r.fields {
            match &f.name {
                Some(n) if n == name => return Some((f.offset, f.clone())),
                None if self.is_record(f.ty) => {
                    if let Some((off, fld)) = self.find_field(f.ty, name) {
                        return Some((f.offset + off, fld));
                    }
                }
                _ => {}
            }
        }
        None
    }

    /// Compatible types (C17 6.2.7), ignoring top-level qualifiers when asked.
    pub fn compatible(&self, a: Ty, b: Ty) -> bool {
        if a == b {
            return true;
        }
        if self.quals(a) != self.quals(b) {
            return false;
        }
        match (self.kind(a), self.kind(b)) {
            (TyKind::Ptr(x), TyKind::Ptr(y)) => self.compatible(*x, *y),
            (TyKind::Array(x, n), TyKind::Array(y, m)) => {
                self.compatible(*x, *y) && (n.is_none() || m.is_none() || n == m)
            }
            (TyKind::Func(f), TyKind::Func(g)) => {
                if !self.compatible(f.ret, g.ret) {
                    return false;
                }
                if f.proto && g.proto {
                    f.variadic == g.variadic
                        && f.params.len() == g.params.len()
                        && f.params.iter().zip(&g.params).all(|(x, y)| self.compatible_unqual(*x, *y))
                } else {
                    true
                }
            }
            (TyKind::Enum(_), TyKind::Int(IntKind::Int)) | (TyKind::Int(IntKind::Int), TyKind::Enum(_)) => true,
            _ => false,
        }
    }

    pub fn compatible_unqual(&self, a: Ty, b: Ty) -> bool {
        let ka = TyData { kind: self.kind(a).clone(), quals: 0 };
        let kb = TyData { kind: self.kind(b).clone(), quals: 0 };
        match (self.map.get(&ka), self.map.get(&kb)) {
            (Some(&x), Some(&y)) => self.compatible(x, y),
            _ => false,
        }
    }

    pub fn display(&self, t: Ty) -> String {
        let mut s = String::new();
        self.fmt(t, &mut s);
        s
    }

    fn fmt(&self, t: Ty, s: &mut String) {
        let q = self.quals(t);
        if q & Q_CONST != 0 {
            s.push_str("const ");
        }
        if q & Q_VOLATILE != 0 {
            s.push_str("volatile ");
        }
        match self.kind(t) {
            TyKind::Void => s.push_str("void"),
            TyKind::Bool => s.push_str("_Bool"),
            TyKind::Int(k) => s.push_str(match k {
                IntKind::Char => "char",
                IntKind::SChar => "signed char",
                IntKind::UChar => "unsigned char",
                IntKind::Short => "short",
                IntKind::UShort => "unsigned short",
                IntKind::Int => "int",
                IntKind::UInt => "unsigned int",
                IntKind::Long => "long",
                IntKind::ULong => "unsigned long",
                IntKind::LongLong => "long long",
                IntKind::ULongLong => "unsigned long long",
            }),
            TyKind::Float(k) => s.push_str(match k {
                FloatKind::Float => "float",
                FloatKind::Double => "double",
                FloatKind::LongDouble => "long double",
            }),
            TyKind::Ptr(p) => {
                self.fmt(*p, s);
                s.push_str(" *");
            }
            TyKind::Array(e, n) => {
                self.fmt(*e, s);
                match n {
                    Some(n) => {
                        let _ = write!(s, "[{}]", n);
                    }
                    None => s.push_str("[]"),
                }
            }
            TyKind::Func(f) => {
                self.fmt(f.ret, s);
                s.push_str(" (");
                for (i, p) in f.params.iter().enumerate() {
                    if i > 0 {
                        s.push_str(", ");
                    }
                    self.fmt(*p, s);
                }
                if f.variadic {
                    s.push_str(", ...");
                }
                s.push(')');
            }
            TyKind::Record(r) => {
                let r = &self.records[r.0 as usize];
                s.push_str(if r.is_union { "union " } else { "struct " });
                s.push_str(r.name.as_deref().unwrap_or("<anonymous>"));
            }
            TyKind::Enum(e) => {
                s.push_str("enum ");
                s.push_str(self.enums[e.0 as usize].name.as_deref().unwrap_or("<anonymous>"));
            }
        }
    }
}
