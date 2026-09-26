//! `-E` output: tokens back to text, one source line per output line.

use crate::lex;
use crate::token::{Token, TokenKind};

/// Prints a preprocessed token stream. Tokens keep their line structure;
/// a space is inserted wherever two adjacent spellings would otherwise lex
/// as different tokens.
pub fn print_tokens(toks: &[Token]) -> String {
    let mut out = String::new();
    let mut prev: Option<&Token> = None;
    for t in toks {
        if t.kind == TokenKind::Eof {
            break;
        }
        if t.kind == TokenKind::Pragma {
            if !out.is_empty() && !out.ends_with('\n') {
                out.push('\n');
            }
            out.push_str("#pragma ");
            out.push_str(&t.text);
            out.push('\n');
            prev = None;
            continue;
        }
        if let Some(p) = prev {
            if t.bol {
                out.push('\n');
            } else if t.space || needs_space(p, t) {
                out.push(' ');
            }
        } else if t.space && !out.is_empty() && !out.ends_with('\n') {
            out.push(' ');
        }
        out.push_str(&t.text);
        prev = Some(t);
    }
    if !out.is_empty() && !out.ends_with('\n') {
        out.push('\n');
    }
    out
}

fn needs_space(a: &Token, b: &Token) -> bool {
    let joined = format!("{}{}", a.text, b.text);
    let toks = lex::tokenize_fragment(&joined, a.loc);
    !(toks.len() == 2 && *toks[0].text == *a.text && *toks[1].text == *b.text)
}
