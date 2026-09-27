//! 65816 machine instructions as the backend emits them: mnemonic plus a
//! typed operand, so sizes are known (branch relaxation) and a peephole pass
//! can reason about them. Printed in WLA-DX syntax.

use std::fmt::Write as _;

/// An operand expression: a symbol plus a constant, or a plain number.
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum Expr {
    Num(i64),
    Sym(String, i64),
    /// The bank byte of a symbol (`:sym`).
    Bank(String),
}

impl Expr {
    pub fn text(&self) -> String {
        match self {
            Expr::Num(n) => {
                if *n < 0 {
                    format!("{}", n)
                } else {
                    format!("${:x}", n)
                }
            }
            Expr::Sym(s, 0) => s.clone(),
            Expr::Sym(s, o) if *o > 0 => format!("{}+{}", s, o),
            Expr::Sym(s, o) => format!("{}-{}", s, -o),
            Expr::Bank(s) => format!(":{}", s),
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum Mode {
    Implied,
    Acc,
    /// `#imm` (width from the M/X flag the instruction runs under).
    Imm(Expr),
    Dp(u8),
    DpX(u8),
    /// `(dp)`
    DpInd(u8),
    /// `(dp),y`
    DpIndY(u8),
    /// `[dp]`
    DpIndLong(u8),
    /// `[dp],y`
    DpIndLongY(u8),
    Abs(Expr),
    AbsX(Expr),
    AbsY(Expr),
    Long(Expr),
    LongX(Expr),
    /// `n,s`
    Sr(u8),
    /// A branch/jump target label.
    Label(String),
    /// `(abs,x)` for jmp tables.
    AbsIndX(Expr),
    /// Block move source/destination banks.
    Move(Expr, Expr),
}

#[derive(Clone, Debug, PartialEq)]
pub enum Line {
    Label(String),
    Inst { mnem: &'static str, mode: Mode, wide_imm: bool },
    /// Assembler directive or data, printed verbatim.
    Raw(String),
    /// Data inside code (jump tables): text and byte size.
    Data(String, u32),
    Comment(String),
}

impl Line {
    pub fn inst(mnem: &'static str, mode: Mode) -> Line {
        Line::Inst { mnem, mode, wide_imm: true }
    }

    pub fn is_branch(&self) -> bool {
        matches!(self, Line::Inst { mnem, .. } if is_cond_branch(mnem) || *mnem == "bra" || *mnem == "brl")
    }
}

pub fn is_cond_branch(m: &str) -> bool {
    matches!(m, "bcc" | "bcs" | "beq" | "bne" | "bmi" | "bpl" | "bvc" | "bvs")
}

pub fn invert_branch(m: &str) -> &'static str {
    match m {
        "bcc" => "bcs",
        "bcs" => "bcc",
        "beq" => "bne",
        "bne" => "beq",
        "bmi" => "bpl",
        "bpl" => "bmi",
        "bvc" => "bvs",
        "bvs" => "bvc",
        _ => unreachable!(),
    }
}

/// Byte size of an instruction.
pub fn size(mnem: &str, mode: &Mode, wide_imm: bool) -> u32 {
    match mode {
        Mode::Implied | Mode::Acc => 1,
        Mode::Imm(_) => {
            if matches!(mnem, "sep" | "rep") {
                2
            } else if wide_imm {
                3
            } else {
                2
            }
        }
        Mode::Dp(_) | Mode::DpX(_) | Mode::DpInd(_) | Mode::DpIndY(_) | Mode::DpIndLong(_) | Mode::DpIndLongY(_) | Mode::Sr(_) => 2,
        Mode::Abs(_) | Mode::AbsX(_) | Mode::AbsY(_) | Mode::AbsIndX(_) | Mode::Move(..) => 3,
        Mode::Long(_) | Mode::LongX(_) => 4,
        Mode::Label(_) => match mnem {
            "brl" | "jmp" | "jsr" | "per" => 3,
            "jml" | "jsl" => 4,
            _ => 2,
        },
    }
}

/// Approximate cycle count (native mode, 16-bit memory/index, direct page
/// aligned); used only for reporting estimates.
pub fn cycles(mnem: &str, mode: &Mode, wide: bool) -> u32 {
    let m = if wide { 1 } else { 0 };
    match mnem {
        "sep" | "rep" => 3,
        "clc" | "sec" | "tax" | "tay" | "txa" | "tya" | "inx" | "iny" | "dex" | "dey" | "xba" | "tcd" | "tdc" | "tsa" | "tas" | "txy" | "tyx" | "nop" => 2,
        "pha" | "phx" | "phy" => 3 + m,
        "pla" | "plx" | "ply" => 4 + m,
        "rtl" => 6,
        "rts" => 6,
        "jsl" => 8,
        "jsr" => 6,
        "bra" => 3,
        "brl" => 4,
        "jmp" => 3,
        "jml" => 4,
        _ if is_cond_branch(mnem) => 2,
        _ => match mode {
            Mode::Imm(_) => 2 + m,
            Mode::Dp(_) => 3 + m,
            Mode::DpX(_) => 4 + m,
            Mode::DpInd(_) => 5 + m,
            Mode::DpIndY(_) => 6 + m,
            Mode::DpIndLong(_) => 6 + m,
            Mode::DpIndLongY(_) => 6 + m,
            Mode::Abs(_) => 4 + m,
            Mode::AbsX(_) | Mode::AbsY(_) => 5 + m,
            Mode::Long(_) => 5 + m,
            Mode::LongX(_) => 5 + m,
            Mode::Sr(_) => 4 + m,
            Mode::Acc => 2,
            _ => 3,
        },
    }
}

fn mode_text(mode: &Mode) -> String {
    match mode {
        Mode::Implied => String::new(),
        Mode::Acc => "a".into(),
        Mode::Imm(e) => format!("#{}", e.text()),
        Mode::Dp(d) => format!("${:02x}", d),
        Mode::DpX(d) => format!("${:02x},x", d),
        Mode::DpInd(d) => format!("(${:02x})", d),
        Mode::DpIndY(d) => format!("(${:02x}),y", d),
        Mode::DpIndLong(d) => format!("[${:02x}]", d),
        Mode::DpIndLongY(d) => format!("[${:02x}],y", d),
        Mode::Abs(e) => e.text(),
        Mode::AbsX(e) => format!("{},x", e.text()),
        Mode::AbsY(e) => format!("{},y", e.text()),
        Mode::Long(e) => e.text(),
        Mode::LongX(e) => format!("{},x", e.text()),
        Mode::Sr(n) => format!("{},s", n),
        Mode::Label(l) => l.clone(),
        Mode::AbsIndX(e) => format!("({},x)", e.text()),
        Mode::Move(a, b) => format!("{},{}", a.text(), b.text()),
    }
}

/// The size suffix WLA needs to pick the addressing mode.
fn suffix(mnem: &str, mode: &Mode) -> &'static str {
    match mode {
        Mode::Dp(_) | Mode::DpX(_) => ".b",
        Mode::Abs(_) | Mode::AbsX(_) | Mode::AbsY(_) => {
            if matches!(mnem, "jmp" | "jsr") {
                ""
            } else {
                ".w"
            }
        }
        Mode::Long(_) | Mode::LongX(_) => ".l",
        _ => "",
    }
}

pub fn print_line(l: &Line, out: &mut String) {
    match l {
        Line::Label(s) => {
            let _ = writeln!(out, "{}:", s);
        }
        Line::Inst { mnem, mode, wide_imm } => {
            let t = mode_text(mode);
            let sfx = suffix(mnem, mode);
            // Immediate width: WLA infers from .accu/.index; be explicit.
            let sfx = match (mode, mnem) {
                (Mode::Imm(_), &"sep") | (Mode::Imm(_), &"rep") => "",
                (Mode::Imm(_), _) => {
                    if *wide_imm {
                        ".w"
                    } else {
                        ".b"
                    }
                }
                _ => sfx,
            };
            if t.is_empty() {
                let _ = writeln!(out, "  {}{}", mnem, sfx);
            } else {
                let _ = writeln!(out, "  {}{} {}", mnem, sfx, t);
            }
        }
        Line::Raw(s) | Line::Data(s, _) => {
            let _ = writeln!(out, "{}", s);
        }
        Line::Comment(s) => {
            let _ = writeln!(out, "  ; {}", s);
        }
    }
}

/// Replaces out-of-range short branches: `bcc far` becomes `bcs +; brl far`
/// (emitted as `bcs __rN` / `brl far` / `__rN:`), `bra far` becomes `brl`.
pub fn relax_branches(lines: &mut Vec<Line>, fresh: &mut u32) {
    // Relaxing a branch only lengthens the code, so a branch out of range
    // in one layout stays out of range in every later one: relax every such
    // branch in one pass and repeat until none is left (the same fixed
    // point as relaxing one at a time, in linear passes).
    loop {
        // Label addresses.
        let mut pos = 0u32;
        let mut label_at = std::collections::HashMap::new();
        let mut at = Vec::with_capacity(lines.len());
        for l in lines.iter() {
            at.push(pos);
            match l {
                Line::Label(s) => {
                    label_at.insert(s.clone(), pos);
                }
                Line::Inst { mnem, mode, wide_imm } => pos += size(mnem, mode, *wide_imm),
                Line::Data(_, n) => pos += n,
                _ => {}
            }
        }
        let far = |i: usize, l: &Line| -> bool {
            if let Line::Inst { mnem, mode: Mode::Label(target), .. } = l {
                if is_cond_branch(mnem) || *mnem == "bra" {
                    if let Some(&t) = label_at.get(target) {
                        let d = t as i64 - (at[i] + 2) as i64;
                        return !(-128..=127).contains(&d);
                    }
                }
            }
            false
        };
        if !lines.iter().enumerate().any(|(i, l)| far(i, l)) {
            return;
        }
        let mut out = Vec::with_capacity(lines.len() + 16);
        for (i, l) in lines.iter().enumerate() {
            if !far(i, l) {
                out.push(l.clone());
                continue;
            }
            let Line::Inst { mnem, mode: Mode::Label(target), .. } = l else { unreachable!() };
            if *mnem == "bra" {
                out.push(Line::inst("brl", Mode::Label(target.clone())));
            } else {
                *fresh += 1;
                let skip = format!("__lr{}", fresh);
                out.push(Line::inst(invert_branch(mnem), Mode::Label(skip.clone())));
                out.push(Line::inst("brl", Mode::Label(target.clone())));
                out.push(Line::Label(skip));
            }
        }
        *lines = out;
    }
}
