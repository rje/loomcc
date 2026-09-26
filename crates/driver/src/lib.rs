//! loomcc's compilation pipeline, shared by the CLI and the tests.

pub mod testbed;

use loomcc_pp::{Diag, Level, Options, Preprocessor, SourceMap, Token};
use std::path::Path;

pub struct Preprocessed {
    pub tokens: Vec<Token>,
    pub sources: SourceMap,
    pub diags: Vec<Diag>,
}

impl Preprocessed {
    pub fn has_errors(&self) -> bool {
        self.diags.iter().any(|d| d.level == Level::Error)
    }

    pub fn render(&self, diags: &[Diag]) -> String {
        diags.iter().map(|d| self.sources.render(d) + "\n").collect()
    }
}

pub fn preprocess(path: &Path, opts: &Options) -> Result<Preprocessed, String> {
    let mut pp = Preprocessor::new(opts.clone());
    let tokens = pp.run_file(path)?;
    Ok(Preprocessed { tokens, diags: std::mem::take(&mut pp.diags), sources: std::mem::take(&mut pp.sources) })
}

pub struct Parsed {
    pub pre: Preprocessed,
    pub unit: loomcc_parse::ast::TranslationUnit,
    pub diags: Vec<Diag>,
}

pub fn parse(path: &Path, opts: &Options) -> Result<Parsed, String> {
    let pre = preprocess(path, opts)?;
    let (unit, diags) = loomcc_parse::parse(&pre.tokens);
    Ok(Parsed { pre, unit, diags })
}
