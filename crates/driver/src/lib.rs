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

pub struct Checked {
    pub parsed: Parsed,
    pub unit: loomcc_sema::hir::Unit,
    pub diags: Vec<Diag>,
}

impl Checked {
    /// Every diagnostic (preprocessor, parser, sema), rendered.
    pub fn render_all(&self) -> String {
        let p = &self.parsed;
        let mut s = p.pre.render(&p.pre.diags);
        s.push_str(&p.pre.render(&p.diags));
        s.push_str(&p.pre.render(&self.diags));
        s
    }

    pub fn has_errors(&self) -> bool {
        self.parsed.pre.has_errors()
            || self.parsed.diags.iter().chain(&self.diags).any(|d| d.level == Level::Error)
    }
}

pub fn check(path: &Path, opts: &Options) -> Result<Checked, String> {
    let parsed = parse(path, opts)?;
    let name = path.file_name().map(|n| n.to_string_lossy().to_string()).unwrap_or_default();
    let (unit, diags) = loomcc_sema::check(&parsed.unit, &name, loomcc_sema::types::Layout::snes());
    Ok(Checked { parsed, unit, diags })
}
