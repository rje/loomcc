//! Translation phases 1-3: line splicing, comments, preprocessing tokens.

use crate::token::{Loc, Punct, Token, TokenKind, PUNCTS};
use crate::Diag;

/// The file's text after phase 2 (backslash-newline removed), with the
/// original line and column of every byte.
struct Spliced {
    bytes: Vec<u8>,
    lines: Vec<u32>,
    cols: Vec<u32>,
}

fn splice(text: &str) -> Spliced {
    let src = text.as_bytes();
    let mut bytes = Vec::with_capacity(src.len() + 1);
    let mut lines = Vec::with_capacity(src.len() + 1);
    let mut cols = Vec::with_capacity(src.len() + 1);
    let (mut line, mut col) = (1u32, 1u32);
    let mut i = 0;
    while i < src.len() {
        let c = src[i];
        if c == b'\\' {
            // Backslash, optional horizontal whitespace (a common extension),
            // then a newline: a splice.
            let mut j = i + 1;
            while j < src.len() && (src[j] == b' ' || src[j] == b'\t') {
                j += 1;
            }
            if j < src.len() && (src[j] == b'\n' || src[j] == b'\r') {
                if src[j] == b'\r' && j + 1 < src.len() && src[j + 1] == b'\n' {
                    j += 1;
                }
                i = j + 1;
                line += 1;
                col = 1;
                continue;
            }
        }
        if c == b'\r' {
            // CRLF or lone CR: one newline.
            if i + 1 < src.len() && src[i + 1] == b'\n' {
                i += 1;
            }
            bytes.push(b'\n');
            lines.push(line);
            cols.push(col);
            line += 1;
            col = 1;
            i += 1;
            continue;
        }
        bytes.push(c);
        lines.push(line);
        cols.push(col);
        if c == b'\n' {
            line += 1;
            col = 1;
        } else {
            col += 1;
        }
        i += 1;
    }
    // A file that does not end in a newline behaves as if it did.
    if bytes.last().map_or(false, |&b| b != b'\n') {
        bytes.push(b'\n');
        lines.push(line);
        cols.push(col);
    }
    lines.push(line);
    cols.push(col);
    Spliced { bytes, lines, cols }
}

pub(crate) fn is_ident_start(c: u8) -> bool {
    c.is_ascii_alphabetic() || c == b'_' || c == b'$' || c >= 0x80
}

pub(crate) fn is_ident_continue(c: u8) -> bool {
    is_ident_start(c) || c.is_ascii_digit()
}

/// Lexes one file into preprocessing tokens, ending with an `Eof` token.
/// Newlines are not tokens: a token that starts a line has `bol` set.
pub fn tokenize(text: &str, file: u32, diags: &mut Vec<Diag>) -> Vec<Token> {
    let body = text.trim_end_matches(['\n', '\r']);
    if body.ends_with('\\') {
        let line = body.matches('\n').count() as u32 + 1;
        diags.push(Diag::warning(Loc { file, line, col: 1 }, "backslash-newline at end of file"));
    }
    let s = splice(text);
    let b = &s.bytes;
    let n = b.len();
    let mut out = Vec::new();
    let mut i = 0;
    let mut bol = true;
    let mut space = false;
    let loc_at = |i: usize| Loc {
        file,
        line: s.lines[i.min(s.lines.len() - 1)],
        col: s.cols[i.min(s.cols.len() - 1)],
    };
    // Header names are lexed only right after `#include`/`#import`/etc.; track
    // the line's tokens so far.
    let mut line_tokens: Vec<(TokenKind, std::rc::Rc<str>)> = Vec::new();
    while i < n {
        let c = b[i];
        if c == b'\n' {
            bol = true;
            space = false;
            line_tokens.clear();
            i += 1;
            continue;
        }
        if c == b' ' || c == b'\t' || c == 0x0b || c == 0x0c {
            space = true;
            i += 1;
            continue;
        }
        if c == b'/' && i + 1 < n && b[i + 1] == b'/' {
            while i < n && b[i] != b'\n' {
                i += 1;
            }
            space = true;
            continue;
        }
        if c == b'/' && i + 1 < n && b[i + 1] == b'*' {
            let start = i;
            i += 2;
            loop {
                if i + 1 >= n {
                    diags.push(Diag::error(loc_at(start), "unterminated comment"));
                    i = n;
                    break;
                }
                if b[i] == b'*' && b[i + 1] == b'/' {
                    i += 2;
                    break;
                }
                i += 1;
            }
            space = true;
            continue;
        }
        let start = i;
        let loc = loc_at(start);
        let text_of = |a: usize, z: usize| String::from_utf8_lossy(&b[a..z]).into_owned();

        // Header name after `# include`.
        let in_include = line_tokens.len() == 2
            && line_tokens[0].0 == TokenKind::Punct(Punct::Hash)
            && line_tokens[1].0 == TokenKind::Ident
            && matches!(
                &*line_tokens[1].1,
                "include" | "include_next" | "import" | "embed"
            );
        let kind;
        if in_include && c == b'<' {
            let mut j = i + 1;
            while j < n && b[j] != b'>' && b[j] != b'\n' {
                j += 1;
            }
            if j < n && b[j] == b'>' {
                i = j + 1;
                // Represent `<...>` as a string-like token spelled with angle
                // brackets; the directive handler recognises it.
                kind = TokenKind::Str;
                push(&mut out, &mut line_tokens, kind, text_of(start, i), loc, &mut bol, &mut space);
                continue;
            }
        }

        if is_ident_start(c) || (c == b'\\' && i + 1 < n && (b[i + 1] == b'u' || b[i + 1] == b'U')) {
            // String/char prefixes.
            let mut j = i;
            while j < n && (is_ident_continue(b[j]) || (b[j] == b'\\' && j + 1 < n && (b[j + 1] == b'u' || b[j + 1] == b'U'))) {
                if b[j] == b'\\' {
                    j += 2;
                } else {
                    j += 1;
                }
            }
            let word = &b[i..j];
            if j < n && (b[j] == b'"' || b[j] == b'\'') && matches!(word, b"L" | b"u" | b"U" | b"u8") {
                let quote = b[j];
                match scan_quoted(b, j, quote) {
                    Some(end) => {
                        i = end;
                        kind = if quote == b'"' { TokenKind::Str } else { TokenKind::Char };
                    }
                    None => {
                        diags.push(Diag::error(loc, "missing terminating quote"));
                        i = line_end(b, j);
                        kind = TokenKind::Other;
                    }
                }
            } else {
                i = j;
                kind = TokenKind::Ident;
            }
        } else if c.is_ascii_digit() || (c == b'.' && i + 1 < n && b[i + 1].is_ascii_digit()) {
            let mut j = i + 1;
            loop {
                if j >= n {
                    break;
                }
                let d = b[j];
                if (d == b'+' || d == b'-') && matches!(b[j - 1], b'e' | b'E' | b'p' | b'P') {
                    j += 1;
                } else if d.is_ascii_alphanumeric() || d == b'_' || d == b'.' {
                    j += 1;
                } else if d == b'\'' && j + 1 < n && b[j + 1].is_ascii_alphanumeric() {
                    // C23 digit separator.
                    j += 1;
                } else {
                    break;
                }
            }
            i = j;
            kind = TokenKind::Number;
        } else if c == b'"' || c == b'\'' {
            match scan_quoted(b, i, c) {
                Some(end) => {
                    i = end;
                    kind = if c == b'"' { TokenKind::Str } else { TokenKind::Char };
                }
                None => {
                    // An unmatched quote is a stray character in skipped
                    // groups; as a real token it is an error the parser reports.
                    i += 1;
                    kind = TokenKind::Other;
                }
            }
        } else {
            let mut matched = None;
            for (spelling, p) in PUNCTS {
                let sb = spelling.as_bytes();
                if b[i..].starts_with(sb) {
                    // `<::` is not `<:` followed by `:` in C++, but in C it is.
                    matched = Some((sb.len(), *p));
                    break;
                }
            }
            match matched {
                Some((len, p)) => {
                    i += len;
                    kind = TokenKind::Punct(p);
                }
                None => {
                    // One (possibly multi-byte UTF-8) character.
                    let mut j = i + 1;
                    while j < n && (b[j] & 0xc0) == 0x80 {
                        j += 1;
                    }
                    i = j;
                    kind = TokenKind::Other;
                }
            }
        }
        push(&mut out, &mut line_tokens, kind, text_of(start, i), loc, &mut bol, &mut space);
    }
    let mut eof = Token::new(TokenKind::Eof, "", loc_at(n));
    eof.bol = true;
    out.push(eof);
    out
}

fn push(
    out: &mut Vec<Token>,
    line_tokens: &mut Vec<(TokenKind, std::rc::Rc<str>)>,
    kind: TokenKind,
    text: String,
    loc: Loc,
    bol: &mut bool,
    space: &mut bool,
) {
    let mut t = Token::new(kind, text, loc);
    t.bol = *bol;
    t.space = *space;
    *bol = false;
    *space = false;
    if line_tokens.len() < 3 {
        line_tokens.push((t.kind, t.text.clone()));
    }
    out.push(t);
}

fn line_end(b: &[u8], mut i: usize) -> usize {
    while i < b.len() && b[i] != b'\n' {
        i += 1;
    }
    i
}

/// Returns the index just past the closing quote, or None when the line ends
/// first.
fn scan_quoted(b: &[u8], start: usize, quote: u8) -> Option<usize> {
    let mut i = start + 1;
    while i < b.len() {
        match b[i] {
            b'\\' => i += 2,
            b'\n' => return None,
            c if c == quote => return Some(i + 1),
            _ => i += 1,
        }
    }
    None
}

/// Lexes a single spelling (used by `##` pasting and `_Pragma`). Returns the
/// tokens without the trailing Eof.
pub fn tokenize_fragment(text: &str, loc: Loc) -> Vec<Token> {
    let mut diags = Vec::new();
    let mut toks = tokenize(text, loc.file, &mut diags);
    toks.pop();
    for t in &mut toks {
        t.loc = loc;
    }
    toks
}

#[cfg(test)]
mod tests {
    use super::*;

    fn spell(src: &str) -> Vec<String> {
        let mut d = Vec::new();
        tokenize(src, 0, &mut d)
            .into_iter()
            .filter(|t| t.kind != TokenKind::Eof)
            .map(|t| t.text.to_string())
            .collect()
    }

    #[test]
    fn basic_tokens() {
        assert_eq!(
            spell("int x = a->b + 0x1fu; /* c */ s = \"a\\\"b\" 'c' L'd' u8\"e\""),
            vec!["int", "x", "=", "a", "->", "b", "+", "0x1fu", ";", "s", "=", "\"a\\\"b\"", "'c'", "L'd'", "u8\"e\""]
        );
    }

    #[test]
    fn splices_and_numbers() {
        assert_eq!(spell("ab\\\ncd 1.5e+3 .5 1..2 x+++y"), vec!["abcd", "1.5e+3", ".5", "1..2", "x", "++", "+", "y"]);
    }

    #[test]
    fn digraphs_and_header_names() {
        assert_eq!(spell("<: :> <% %> %: %:%:"), vec!["<:", ":>", "<%", "%>", "%:", "%:%:"]);
        assert_eq!(spell("#include <a/b.h>\n< x >"), vec!["#", "include", "<a/b.h>", "<", "x", ">"]);
    }

    #[test]
    fn line_flags() {
        let mut d = Vec::new();
        let t = tokenize("a b\n  c", 0, &mut d);
        assert!(t[0].bol && !t[1].bol && t[1].space && t[2].bol);
        assert_eq!((t[2].loc.line, t[2].loc.col), (2, 3));
    }
}
