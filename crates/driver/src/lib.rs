//! loomcc's compilation pipeline, shared by the CLI and the tests.

pub mod testbed;

/// loomcc's own freestanding headers (stddef.h, stdint.h, stdio.h, ...).
pub fn builtin_include_dir() -> std::path::PathBuf {
    std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("include")
}

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

/// Every diagnostic from compiling a set of files.
pub struct Compiled {
    pub module: Option<loomcc_ir::Module>,
    pub messages: String,
    pub failed: bool,
}

/// Front end + lowering for a whole program (all translation units at once).
pub fn compile_ir(paths: &[std::path::PathBuf], opts: &Options) -> Compiled {
    compile_ir_prefixed(paths, opts, "", 2)
}

pub fn compile_ir_prefixed(paths: &[std::path::PathBuf], opts: &Options, prefix: &str, opt_level: u8) -> Compiled {
    let mut units = Vec::new();
    let mut sources = Vec::new();
    let mut messages = String::new();
    let mut failed = false;
    for p in paths {
        match check(p, opts) {
            Ok(c) => {
                messages.push_str(&c.render_all());
                failed |= c.has_errors();
                sources.push(c.parsed.pre.sources);
                units.push(c.unit);
            }
            Err(e) => {
                messages.push_str(&format!("loomcc: {}\n", e));
                failed = true;
            }
        }
    }
    if failed {
        return Compiled { module: None, messages, failed };
    }
    let (m, diags) = loomcc_ir::lower::lower_units_prefixed(&units, prefix);
    for (ui, d) in &diags {
        messages.push_str(&sources[*ui].render(d));
        messages.push('\n');
        failed |= d.level == Level::Error;
    }
    let mut m = m;
    if !failed {
        loomcc_opt::optimize_module(&mut m, &loomcc_opt::Options { level: opt_level, inline: true });
    }
    if let Err(e) = loomcc_ir::verify::verify_module(&m) {
        messages.push_str(&format!("loomcc: internal IR error: {}\n", e));
        failed = true;
    }
    Compiled { module: Some(m), messages, failed }
}
