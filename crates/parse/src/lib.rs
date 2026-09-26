//! loomcc-parse: the C17 parser.

pub mod ast;
pub mod parser;
pub mod print;

use loomcc_pp::{Diag, Token};

/// Parses a preprocessed token stream (ending in Eof).
pub fn parse(tokens: &[Token]) -> (ast::TranslationUnit, Vec<Diag>) {
    let mut p = parser::Parser::new(tokens);
    let tu = p.translation_unit();
    (tu, p.diags)
}

#[cfg(test)]
mod tests;
