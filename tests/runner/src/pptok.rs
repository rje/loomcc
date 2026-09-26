//! A small C preprocessing-token lexer used to compare `-E` outputs
//! without caring about whitespace or line structure.

pub fn is_ident_start(c: u8) -> bool {
    c.is_ascii_alphabetic() || c == b'_' || c == b'$' || c >= 0x80
}

fn is_ident_cont(c: u8) -> bool {
    is_ident_start(c) || c.is_ascii_digit()
}

const PUNCT: &[&str] = &[
    "%:%:", "...", "<<=", ">>=", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", "&&", "||", "*=", "/=", "%=",
    "+=", "-=", "&=", "^=", "|=", "##", "<:", ":>", "<%", "%>", "%:",
];

/// Splits `text` into preprocessing tokens. Comments are skipped (they
/// should not appear in `-E` output, but expected text may carry them).
pub fn tokenize(text: &str) -> Vec<String> {
    let b = text.as_bytes();
    let mut i = 0;
    let mut out = Vec::new();
    while i < b.len() {
        let c = b[i];
        if c.is_ascii_whitespace() {
            i += 1;
            continue;
        }
        if c == b'\\' && i + 1 < b.len() && b[i + 1] == b'\n' {
            i += 2;
            continue;
        }
        if c == b'/' && i + 1 < b.len() && b[i + 1] == b'*' {
            match text[i + 2..].find("*/") {
                Some(e) => i = i + 2 + e + 2,
                None => i = b.len(),
            }
            continue;
        }
        if c == b'/' && i + 1 < b.len() && b[i + 1] == b'/' {
            while i < b.len() && b[i] != b'\n' {
                i += 1;
            }
            continue;
        }
        let start = i;
        // String and character literals, with encoding prefixes.
        let prefix_len = ["u8", "u", "U", "L"]
            .iter()
            .find(|p| text[i..].starts_with(*p) && matches!(b.get(i + p.len()), Some(b'"') | Some(b'\'')))
            .map(|p| p.len());
        if let Some(pl) = prefix_len.or(if c == b'"' || c == b'\'' { Some(0) } else { None }) {
            let q = b[i + pl];
            i += pl + 1;
            while i < b.len() && b[i] != q && b[i] != b'\n' {
                if b[i] == b'\\' && i + 1 < b.len() {
                    i += 1;
                }
                i += 1;
            }
            if i < b.len() && b[i] == q {
                i += 1;
            }
            out.push(text[start..i].to_string());
            continue;
        }
        if is_ident_start(c) {
            while i < b.len() && (is_ident_cont(b[i]) || (b[i] == b'\\' && matches!(b.get(i + 1), Some(b'u') | Some(b'U')))) {
                i += if b[i] == b'\\' { 2 } else { 1 };
            }
            out.push(text[start..i].to_string());
            continue;
        }
        if c.is_ascii_digit() || (c == b'.' && i + 1 < b.len() && b[i + 1].is_ascii_digit()) {
            i += 1;
            while i < b.len() {
                let d = b[i];
                if matches!(d, b'+' | b'-') && matches!(b[i - 1], b'e' | b'E' | b'p' | b'P') {
                    i += 1;
                } else if is_ident_cont(d) || d == b'.' {
                    i += 1;
                } else if d == b'\'' && i + 1 < b.len() && is_ident_cont(b[i + 1]) {
                    // C23 digit separator
                    i += 1;
                } else {
                    break;
                }
            }
            out.push(text[start..i].to_string());
            continue;
        }
        if let Some(p) = PUNCT.iter().find(|p| text[i..].starts_with(*p)) {
            i += p.len();
            out.push(p.to_string());
            continue;
        }
        // Any other single character (UTF-8 aware).
        let ch_len = text[i..].chars().next().map(|c| c.len_utf8()).unwrap_or(1);
        i += ch_len;
        out.push(text[start..i].to_string());
    }
    out
}

/// Drops `# <n> "file"` and `#line` markers (816-tcc and clang without -P).
pub fn strip_line_markers(text: &str) -> String {
    let mut out = String::new();
    for line in text.lines() {
        let t = line.trim_start();
        if let Some(rest) = t.strip_prefix('#') {
            let rest = rest.trim_start();
            if rest.starts_with(|c: char| c.is_ascii_digit()) || rest.starts_with("line ") {
                continue;
            }
        }
        out.push_str(line);
        out.push('\n');
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn basics() {
        assert_eq!(tokenize("a+++b"), vec!["a", "++", "+", "b"]);
        assert_eq!(tokenize("u8\"x\" L'a' 0x1.p+3 1e-5.x"), vec!["u8\"x\"", "L'a'", "0x1.p+3", "1e-5.x"]);
        assert_eq!(tokenize("%:%: <::> ..."), vec!["%:%:", "<:", ":>", "..."]);
        assert_eq!(tokenize("\"a\\\"b\" c"), vec!["\"a\\\"b\"", "c"]);
    }
}
