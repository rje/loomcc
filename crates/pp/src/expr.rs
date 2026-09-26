//! `#if` constant expressions, evaluated in intmax_t / uintmax_t (64 bits).

use crate::token::{Punct, Token, TokenKind};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct Val {
    v: u64,
    unsigned: bool,
}

impl Val {
    fn s(v: i64) -> Val {
        Val { v: v as u64, unsigned: false }
    }
    fn truth(self) -> bool {
        self.v != 0
    }
}

struct P<'a> {
    toks: &'a [Token],
    i: usize,
    /// Inside the unevaluated operand of `&&`, `||` or `?:`: division by zero
    /// is not an error there.
    skip: usize,
}

pub fn eval(toks: &[Token]) -> Result<i64, String> {
    if toks.is_empty() {
        return Err("expected value in expression".into());
    }
    let mut p = P { toks, i: 0, skip: 0 };
    let v = p.comma()?;
    if p.i < toks.len() {
        return Err(format!("token \"{}\" is not valid in preprocessor expressions", toks[p.i].text));
    }
    Ok(v.v as i64)
}

impl<'a> P<'a> {
    fn peek(&self) -> Option<&'a Token> {
        self.toks.get(self.i)
    }

    fn eat(&mut self, p: Punct) -> bool {
        if self.peek().map_or(false, |t| t.is_punct(p)) {
            self.i += 1;
            true
        } else {
            false
        }
    }

    fn comma(&mut self) -> Result<Val, String> {
        let mut v = self.cond()?;
        while self.eat(Punct::Comma) {
            if self.skip == 0 {
                return Err("comma operator in operand of #if".into());
            }
            v = self.cond()?;
        }
        Ok(v)
    }

    fn cond(&mut self) -> Result<Val, String> {
        let c = self.binary(0)?;
        if self.eat(Punct::Question) {
            if !c.truth() {
                self.skip += 1;
            }
            let a = self.comma()?;
            if !c.truth() {
                self.skip -= 1;
            }
            if !self.eat(Punct::Colon) {
                return Err("expected ':' in conditional expression".into());
            }
            if c.truth() {
                self.skip += 1;
            }
            let b = self.cond()?;
            if c.truth() {
                self.skip -= 1;
            }
            let unsigned = a.unsigned || b.unsigned;
            let r = if c.truth() { a } else { b };
            return Ok(Val { v: r.v, unsigned });
        }
        Ok(c)
    }

    fn binary(&mut self, min_prec: u8) -> Result<Val, String> {
        let mut lhs = self.unary()?;
        loop {
            let Some(t) = self.peek() else { break };
            let TokenKind::Punct(p) = t.kind else { break };
            let prec = match p {
                Punct::PipePipe => 1,
                Punct::AmpAmp => 2,
                Punct::Pipe => 3,
                Punct::Caret => 4,
                Punct::Amp => 5,
                Punct::EqEq | Punct::Ne => 6,
                Punct::Lt | Punct::Gt | Punct::Le | Punct::Ge => 7,
                Punct::Shl | Punct::Shr => 8,
                Punct::Plus | Punct::Minus => 9,
                Punct::Star | Punct::Slash | Punct::Percent => 10,
                _ => break,
            };
            if prec < min_prec || prec == 0 {
                break;
            }
            self.i += 1;
            if p == Punct::AmpAmp || p == Punct::PipePipe {
                let short = if p == Punct::AmpAmp { !lhs.truth() } else { lhs.truth() };
                if short {
                    self.skip += 1;
                }
                let rhs = self.binary(prec + 1)?;
                if short {
                    self.skip -= 1;
                }
                let r = if p == Punct::AmpAmp { lhs.truth() && rhs.truth() } else { lhs.truth() || rhs.truth() };
                lhs = Val::s(r as i64);
                continue;
            }
            let rhs = self.binary(prec + 1)?;
            lhs = self.apply(p, lhs, rhs)?;
        }
        Ok(lhs)
    }

    fn apply(&self, p: Punct, a: Val, b: Val) -> Result<Val, String> {
        let unsigned = a.unsigned || b.unsigned;
        let (x, y) = (a.v, b.v);
        let sx = x as i64;
        let sy = y as i64;
        let v = match p {
            Punct::Pipe => x | y,
            Punct::Caret => x ^ y,
            Punct::Amp => x & y,
            Punct::EqEq => return Ok(Val::s((x == y) as i64)),
            Punct::Ne => return Ok(Val::s((x != y) as i64)),
            Punct::Lt => return Ok(Val::s(if unsigned { x < y } else { sx < sy } as i64)),
            Punct::Gt => return Ok(Val::s(if unsigned { x > y } else { sx > sy } as i64)),
            Punct::Le => return Ok(Val::s(if unsigned { x <= y } else { sx <= sy } as i64)),
            Punct::Ge => return Ok(Val::s(if unsigned { x >= y } else { sx >= sy } as i64)),
            // Shifts take the left operand's type.
            Punct::Shl => {
                return Ok(Val { v: if y >= 64 { 0 } else { x << y }, unsigned: a.unsigned });
            }
            Punct::Shr => {
                let v = if a.unsigned {
                    if y >= 64 { 0 } else { x >> y }
                } else if y >= 64 {
                    if sx < 0 { u64::MAX } else { 0 }
                } else {
                    (sx >> y) as u64
                };
                return Ok(Val { v, unsigned: a.unsigned });
            }
            Punct::Plus | Punct::Minus | Punct::Star if !unsigned => {
                let r = match p {
                    Punct::Plus => sx.checked_add(sy),
                    Punct::Minus => sx.checked_sub(sy),
                    _ => sx.checked_mul(sy),
                };
                match r {
                    Some(v) => v as u64,
                    None if self.skip > 0 => 0,
                    None => return Err("integer overflow in preprocessor expression".into()),
                }
            }
            Punct::Plus => x.wrapping_add(y),
            Punct::Minus => x.wrapping_sub(y),
            Punct::Star => x.wrapping_mul(y),
            Punct::Slash | Punct::Percent => {
                if y == 0 {
                    if self.skip > 0 {
                        0
                    } else {
                        return Err("division by zero in preprocessor expression".into());
                    }
                } else if unsigned {
                    if p == Punct::Slash { x / y } else { x % y }
                } else if p == Punct::Slash {
                    sx.wrapping_div(sy) as u64
                } else {
                    sx.wrapping_rem(sy) as u64
                }
            }
            _ => unreachable!(),
        };
        Ok(Val { v, unsigned })
    }

    fn unary(&mut self) -> Result<Val, String> {
        let Some(t) = self.peek() else {
            return Err("expected value in expression".into());
        };
        match t.kind {
            TokenKind::Punct(Punct::Plus) => {
                self.i += 1;
                self.unary()
            }
            TokenKind::Punct(Punct::Minus) => {
                self.i += 1;
                let v = self.unary()?;
                Ok(Val { v: v.v.wrapping_neg(), unsigned: v.unsigned })
            }
            TokenKind::Punct(Punct::Tilde) => {
                self.i += 1;
                let v = self.unary()?;
                Ok(Val { v: !v.v, unsigned: v.unsigned })
            }
            TokenKind::Punct(Punct::Bang) => {
                self.i += 1;
                let v = self.unary()?;
                Ok(Val::s((!v.truth()) as i64))
            }
            TokenKind::Punct(Punct::LParen) => {
                self.i += 1;
                let v = self.comma()?;
                if !self.eat(Punct::RParen) {
                    return Err("expected ')' in preprocessor expression".into());
                }
                Ok(v)
            }
            TokenKind::Number => {
                self.i += 1;
                parse_int(&t.text)
            }
            TokenKind::Char => {
                self.i += 1;
                char_value(&t.text).map(|(v, _)| Val::s(v))
            }
            TokenKind::Ident => {
                // Identifiers left after expansion are 0 (`true`/`false` too in C).
                self.i += 1;
                Ok(Val::s(0))
            }
            _ => Err(format!("token \"{}\" is not valid in preprocessor expressions", t.text)),
        }
    }
}

/// Parses an integer pp-number with C suffixes. Returns the value and whether
/// it is unsigned (for `#if`, where every integer is intmax/uintmax).
fn parse_int(s: &str) -> Result<Val, String> {
    let (v, suffix) = parse_integer_literal(s).ok_or_else(|| format!("invalid integer constant \"{}\"", s))?;
    let unsigned = suffix.contains('u') || suffix.contains('U');
    // A decimal/hex value too large for intmax is unsigned.
    Ok(Val { v, unsigned: unsigned || v > i64::MAX as u64 })
}

/// Splits an integer literal into value and suffix. Returns None for floats
/// or malformed numbers.
pub fn parse_integer_literal(s: &str) -> Option<(u64, String)> {
    let s: String = s.chars().filter(|&c| c != '\'').collect();
    let (digits, radix) = if let Some(r) = s.strip_prefix("0x").or_else(|| s.strip_prefix("0X")) {
        (r, 16)
    } else if let Some(r) = s.strip_prefix("0b").or_else(|| s.strip_prefix("0B")) {
        (r, 2)
    } else if s.starts_with('0') && s.len() > 1 {
        (&s[1..], 8)
    } else {
        (&s[..], 10)
    };
    let end = digits.find(|c: char| !c.is_digit(radix)).unwrap_or(digits.len());
    let (num, suffix) = digits.split_at(end);
    let lower = suffix.to_ascii_lowercase();
    if !matches!(
        lower.as_str(),
        "" | "u" | "l" | "ul" | "lu" | "ll" | "ull" | "llu"
    ) {
        return None;
    }
    if num.is_empty() {
        if radix == 8 {
            return Some((0, suffix.to_string()));
        }
        return None;
    }
    let mut v: u64 = 0;
    for c in num.chars() {
        let d = c.to_digit(radix)? as u64;
        v = v.checked_mul(radix as u64)?.checked_add(d)?;
    }
    Some((v, suffix.to_string()))
}

/// Value of a character constant (first character's value for plain char,
/// sign-extended as `char` is signed). Returns (value, prefix).
pub fn char_value(text: &str) -> Result<(i64, String), String> {
    let q = text.find('\'').ok_or("bad character constant")?;
    let prefix = text[..q].to_string();
    let body = &text[q + 1..text.len() - 1];
    let units = decode_escapes(body)?;
    if units.is_empty() {
        return Err("empty character constant".into());
    }
    if prefix.is_empty() && units.len() > 4 {
        return Err("character constant too long for its type".into());
    }
    let limit: u32 = match prefix.as_str() {
        "" | "u8" => 0xff,
        "u" | "L" => 0xffff,
        _ => u32::MAX,
    };
    if units.iter().any(|&u| u > limit) {
        return Err("escape sequence out of range".into());
    }
    if prefix.is_empty() {
        // Multi-character constants: big-endian packing like GCC.
        let mut v: i64 = 0;
        for &u in &units {
            v = (v << 8) | (u as i64 & 0xff);
        }
        if units.len() == 1 {
            v = (units[0] as u8 as i8) as i64;
        }
        Ok((v, prefix))
    } else {
        Ok((units[0] as i64, prefix))
    }
}

/// Decodes the escape sequences of a literal body into code units (bytes for
/// plain literals; code points for others — the caller narrows).
pub fn decode_escapes(body: &str) -> Result<Vec<u32>, String> {
    let mut out = Vec::new();
    let b: Vec<char> = body.chars().collect();
    let mut i = 0;
    while i < b.len() {
        let c = b[i];
        if c != '\\' {
            let mut buf = [0u8; 4];
            for byte in c.encode_utf8(&mut buf).bytes() {
                out.push(byte as u32);
            }
            i += 1;
            continue;
        }
        i += 1;
        let Some(&e) = b.get(i) else {
            return Err("trailing backslash".into());
        };
        i += 1;
        let v = match e {
            'n' => 10,
            't' => 9,
            'r' => 13,
            'a' => 7,
            'b' => 8,
            'f' => 12,
            'v' => 11,
            'e' => 27,
            '\\' => 92,
            '\'' => 39,
            '"' => 34,
            '?' => 63,
            'x' => {
                let mut v: u32 = 0;
                let mut n = 0;
                while i < b.len() && b[i].is_ascii_hexdigit() {
                    v = v.wrapping_mul(16).wrapping_add(b[i].to_digit(16).unwrap());
                    i += 1;
                    n += 1;
                }
                if n == 0 {
                    return Err("\\x used with no following hex digits".into());
                }
                v
            }
            '0'..='7' => {
                let mut v = e.to_digit(8).unwrap();
                let mut n = 1;
                while n < 3 && i < b.len() && b[i].is_digit(8) {
                    v = v * 8 + b[i].to_digit(8).unwrap();
                    i += 1;
                    n += 1;
                }
                v
            }
            'u' | 'U' => {
                let len = if e == 'u' { 4 } else { 8 };
                let mut v: u32 = 0;
                for _ in 0..len {
                    let d = b.get(i).and_then(|c| c.to_digit(16)).ok_or("incomplete universal character name")?;
                    v = v * 16 + d;
                    i += 1;
                }
                let ch = char::from_u32(v).ok_or("invalid universal character")?;
                let mut buf = [0u8; 4];
                for byte in ch.encode_utf8(&mut buf).bytes() {
                    out.push(byte as u32);
                }
                continue;
            }
            other => other as u32,
        };
        out.push(v);
    }
    Ok(out)
}
