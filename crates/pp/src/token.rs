//! Tokens as the preprocessor and the parser see them.

use std::fmt;
use std::rc::Rc;

/// A source position: file index into the [`SourceMap`](crate::SourceMap),
/// 1-based line and column.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub struct Loc {
    pub file: u32,
    pub line: u32,
    pub col: u32,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum Punct {
    LBracket,
    RBracket,
    LParen,
    RParen,
    LBrace,
    RBrace,
    Dot,
    Arrow,
    PlusPlus,
    MinusMinus,
    Amp,
    Star,
    Plus,
    Minus,
    Tilde,
    Bang,
    Slash,
    Percent,
    Shl,
    Shr,
    Lt,
    Gt,
    Le,
    Ge,
    EqEq,
    Ne,
    Caret,
    Pipe,
    AmpAmp,
    PipePipe,
    Question,
    Colon,
    Semi,
    Ellipsis,
    Assign,
    StarAssign,
    SlashAssign,
    PercentAssign,
    PlusAssign,
    MinusAssign,
    ShlAssign,
    ShrAssign,
    AmpAssign,
    CaretAssign,
    PipeAssign,
    Comma,
    Hash,
    HashHash,
}

impl Punct {
    /// Canonical spelling (digraphs map to their ordinary form).
    pub fn as_str(self) -> &'static str {
        use Punct::*;
        match self {
            LBracket => "[",
            RBracket => "]",
            LParen => "(",
            RParen => ")",
            LBrace => "{",
            RBrace => "}",
            Dot => ".",
            Arrow => "->",
            PlusPlus => "++",
            MinusMinus => "--",
            Amp => "&",
            Star => "*",
            Plus => "+",
            Minus => "-",
            Tilde => "~",
            Bang => "!",
            Slash => "/",
            Percent => "%",
            Shl => "<<",
            Shr => ">>",
            Lt => "<",
            Gt => ">",
            Le => "<=",
            Ge => ">=",
            EqEq => "==",
            Ne => "!=",
            Caret => "^",
            Pipe => "|",
            AmpAmp => "&&",
            PipePipe => "||",
            Question => "?",
            Colon => ":",
            Semi => ";",
            Ellipsis => "...",
            Assign => "=",
            StarAssign => "*=",
            SlashAssign => "/=",
            PercentAssign => "%=",
            PlusAssign => "+=",
            MinusAssign => "-=",
            ShlAssign => "<<=",
            ShrAssign => ">>=",
            AmpAssign => "&=",
            CaretAssign => "^=",
            PipeAssign => "|=",
            Comma => ",",
            Hash => "#",
            HashHash => "##",
        }
    }
}

/// Every punctuator spelling, longest first, with digraphs.
pub(crate) const PUNCTS: &[(&str, Punct)] = &[
    ("%:%:", Punct::HashHash),
    ("...", Punct::Ellipsis),
    ("<<=", Punct::ShlAssign),
    (">>=", Punct::ShrAssign),
    ("->", Punct::Arrow),
    ("++", Punct::PlusPlus),
    ("--", Punct::MinusMinus),
    ("<<", Punct::Shl),
    (">>", Punct::Shr),
    ("<=", Punct::Le),
    (">=", Punct::Ge),
    ("==", Punct::EqEq),
    ("!=", Punct::Ne),
    ("&&", Punct::AmpAmp),
    ("||", Punct::PipePipe),
    ("*=", Punct::StarAssign),
    ("/=", Punct::SlashAssign),
    ("%=", Punct::PercentAssign),
    ("+=", Punct::PlusAssign),
    ("-=", Punct::MinusAssign),
    ("&=", Punct::AmpAssign),
    ("^=", Punct::CaretAssign),
    ("|=", Punct::PipeAssign),
    ("##", Punct::HashHash),
    ("<:", Punct::LBracket),
    (":>", Punct::RBracket),
    ("<%", Punct::LBrace),
    ("%>", Punct::RBrace),
    ("%:", Punct::Hash),
    ("[", Punct::LBracket),
    ("]", Punct::RBracket),
    ("(", Punct::LParen),
    (")", Punct::RParen),
    ("{", Punct::LBrace),
    ("}", Punct::RBrace),
    (".", Punct::Dot),
    ("&", Punct::Amp),
    ("*", Punct::Star),
    ("+", Punct::Plus),
    ("-", Punct::Minus),
    ("~", Punct::Tilde),
    ("!", Punct::Bang),
    ("/", Punct::Slash),
    ("%", Punct::Percent),
    ("<", Punct::Lt),
    (">", Punct::Gt),
    ("^", Punct::Caret),
    ("|", Punct::Pipe),
    ("?", Punct::Question),
    (":", Punct::Colon),
    (";", Punct::Semi),
    ("=", Punct::Assign),
    (",", Punct::Comma),
    ("#", Punct::Hash),
];

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TokenKind {
    Ident,
    /// A preprocessing number (converted by the parser).
    Number,
    /// A character constant including its prefix and quotes.
    Char,
    /// A string literal including its prefix and quotes.
    Str,
    Punct(Punct),
    /// A character that forms no other token (`@`, `` ` ``, a lone `\`).
    Other,
    /// A `#pragma` line that survived preprocessing; `text` is the pragma body.
    Pragma,
    Eof,
}

/// The set of macro names a token may no longer expand (Prosser's hide set).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct HideSet(Option<Rc<Vec<Rc<str>>>>);

impl HideSet {
    pub fn contains(&self, name: &str) -> bool {
        match &self.0 {
            None => false,
            Some(names) => names.iter().any(|n| &**n == name),
        }
    }

    pub fn is_empty(&self) -> bool {
        self.0.as_ref().map_or(true, |n| n.is_empty())
    }

    pub fn with(&self, name: &Rc<str>) -> HideSet {
        if self.contains(name) {
            return self.clone();
        }
        let mut names = self.0.as_ref().map(|n| (**n).clone()).unwrap_or_default();
        names.push(name.clone());
        HideSet(Some(Rc::new(names)))
    }

    pub fn union(&self, other: &HideSet) -> HideSet {
        match (&self.0, &other.0) {
            (None, _) => other.clone(),
            (_, None) => self.clone(),
            (Some(a), Some(b)) => {
                let mut names = (**a).clone();
                for n in b.iter() {
                    if !names.iter().any(|m| m == n) {
                        names.push(n.clone());
                    }
                }
                HideSet(Some(Rc::new(names)))
            }
        }
    }

    pub fn intersection(&self, other: &HideSet) -> HideSet {
        match (&self.0, &other.0) {
            (Some(a), Some(b)) => {
                let names: Vec<_> = a.iter().filter(|n| b.contains(n)).cloned().collect();
                if names.is_empty() {
                    HideSet(None)
                } else {
                    HideSet(Some(Rc::new(names)))
                }
            }
            _ => HideSet(None),
        }
    }
}

#[derive(Clone, Debug)]
pub struct Token {
    pub kind: TokenKind,
    pub text: Rc<str>,
    pub loc: Loc,
    /// First token on its (logical) source line.
    pub bol: bool,
    /// Whitespace precedes the token.
    pub space: bool,
    pub hide: HideSet,
    /// Set on a token produced by a macro expansion (the invocation's location
    /// is `loc`).
    pub expanded: bool,
}

impl Token {
    pub fn new(kind: TokenKind, text: impl Into<Rc<str>>, loc: Loc) -> Token {
        Token {
            kind,
            text: text.into(),
            loc,
            bol: false,
            space: false,
            hide: HideSet::default(),
            expanded: false,
        }
    }

    pub fn is_punct(&self, p: Punct) -> bool {
        self.kind == TokenKind::Punct(p)
    }

    pub fn is_ident(&self, name: &str) -> bool {
        self.kind == TokenKind::Ident && &*self.text == name
    }

    pub fn is_eof(&self) -> bool {
        self.kind == TokenKind::Eof
    }
}

impl fmt::Display for Token {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.text)
    }
}
