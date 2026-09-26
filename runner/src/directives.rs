//! Parsing a test file's `loomcc-*` directives.
//!
//! Directives live in comments so every compiler ignores them:
//!
//! ```c
//! // loomcc-do: preprocess
//! // loomcc-options: -DX=1 -Iinc
//! #define f(a) a+1
//! f(2)            // loomcc-expect: 2+1
//! #error stop     // loomcc-error: stop
//! ```
//!
//! README.md is the reference for the full syntax.

use regex::Regex;
use std::path::Path;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Action {
    /// `loomcc -E`: compare tokens and diagnostics.
    Preprocess,
    /// `loomcc -fsyntax-only`: diagnostics only.
    Syntax,
    /// `loomcc -S`: must compile (and assemble, when wla-65816 is there).
    Compile,
    /// Execute: IR interpreter and harness ROM; exit status 0 is a pass.
    Run,
}

impl Action {
    pub fn name(self) -> &'static str {
        match self {
            Action::Preprocess => "preprocess",
            Action::Syntax => "syntax",
            Action::Compile => "compile",
            Action::Run => "run",
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DiagKind {
    Error,
    Warning,
    /// `loomcc-diagnostic`: an error or a warning (a constraint violation
    /// that compilers may reasonably report either way).
    Any,
}

/// Where an expected diagnostic must be reported.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum DiagLine {
    /// A line of a file (None = the test file itself).
    At(Option<String>, u32),
    /// Anywhere, or with no location at all.
    Anywhere,
}

#[derive(Clone, Debug)]
pub struct ExpectedDiag {
    pub kind: DiagKind,
    pub line: DiagLine,
    pub pattern: Option<Regex>,
    pub pattern_text: String,
    /// The directive's own line, for messages.
    pub directive_line: u32,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum IntWidth {
    /// The result is the same with 16- and 32-bit int.
    Agnostic,
    /// The test's expectations hold only with 16-bit int.
    Sixteen,
}

#[derive(Clone, Debug)]
pub struct Test {
    pub action: Action,
    pub options: Vec<String>,
    /// Expected `-E` output text (joined `loomcc-expect` lines or the
    /// `.expected` sidecar); None = output not checked.
    pub expect: Option<String>,
    pub diags: Vec<ExpectedDiag>,
    pub no_warnings: bool,
    /// Reference tools named by `loomcc-ref` (None = the action's default).
    pub refs: Option<Vec<String>>,
    /// `loomcc-ref-diverges: tool reason`
    pub ref_diverges: Vec<(String, String)>,
    pub xfail: Option<String>,
    pub int_width: IntWidth,
    pub timeout: Option<u64>,
    /// Extra C sources compiled by the same compiler and linked in.
    pub extra_sources: Vec<String>,
    /// C sources always compiled by 816-tcc (interop tests).
    pub tcc_sources: Vec<String>,
    /// Hand-written assembly linked in (ROM runs).
    pub asm_sources: Vec<String>,
    /// Emulator frame cap for ROM runs.
    pub max_frames: Option<u32>,
    /// `loomcc-skip-mode: ir rom host ...`: modes that do not apply.
    pub skip_modes: Vec<String>,
}

pub fn parse(path: &Path, text: &str) -> Result<Option<Test>, String> {
    let re = Regex::new(r"(?://|/\*)\s*loomcc-([a-z0-9-]+)(?:@([^\s:]+(?::[+-]?\d+)?))?(?:\s*:\s?(.*?))?\s*(?:\*/\s*)?$").unwrap();
    let mut action = None;
    let mut t = Test {
        action: Action::Preprocess,
        options: Vec::new(),
        expect: None,
        diags: Vec::new(),
        no_warnings: false,
        refs: None,
        ref_diverges: Vec::new(),
        xfail: None,
        int_width: IntWidth::Agnostic,
        timeout: None,
        extra_sources: Vec::new(),
        tcc_sources: Vec::new(),
        asm_sources: Vec::new(),
        max_frames: None,
        skip_modes: Vec::new(),
    };
    let mut expect_lines: Vec<String> = Vec::new();
    let mut have_expect = false;
    let mut int_given = false;
    for (idx, raw) in text.lines().enumerate() {
        let lineno = idx as u32 + 1;
        // A directive must start a comment: find the comment opener before `loomcc-`.
        let Some(pos) = raw.find("loomcc-") else { continue };
        let start = match raw[..pos].rfind("//").into_iter().chain(raw[..pos].rfind("/*")).max() {
            Some(s) => s,
            None => continue,
        };
        let Some(c) = re.captures(&raw[start..]) else { continue };
        let name = c.get(1).unwrap().as_str();
        let at = c.get(2).map(|m| m.as_str());
        let value = c.get(3).map(|m| m.as_str()).unwrap_or("").to_string();
        let err = |m: &str| format!("{}:{}: {}", path.display(), lineno, m);
        match name {
            "do" => {
                action = Some(match value.trim() {
                    "preprocess" => Action::Preprocess,
                    "syntax" => Action::Syntax,
                    "compile" => Action::Compile,
                    "run" => Action::Run,
                    other => return Err(err(&format!("unknown action `{}`", other))),
                })
            }
            "options" => t.options.extend(shell_split(&value).map_err(|e| err(&e))?),
            "expect" => {
                have_expect = true;
                if !value.trim().is_empty() {
                    expect_lines.push(value.clone());
                }
            }
            "error" | "warning" | "diagnostic" => {
                let kind = match name {
                    "error" => DiagKind::Error,
                    "warning" => DiagKind::Warning,
                    _ => DiagKind::Any,
                };
                let line = match at {
                    None => DiagLine::At(None, lineno),
                    Some("*") => DiagLine::Anywhere,
                    Some(s) => parse_at(s, lineno).map_err(|e| err(&e))?,
                };
                let pat = value.trim();
                let pattern = if pat.is_empty() {
                    None
                } else {
                    Some(Regex::new(&format!("(?i){}", pat)).map_err(|e| err(&format!("bad regex: {}", e)))?)
                };
                t.diags.push(ExpectedDiag { kind, line, pattern, pattern_text: pat.to_string(), directive_line: lineno });
            }
            "no-warnings" => t.no_warnings = true,
            "ref" => {
                t.refs = Some(value.split(|c: char| c == ',' || c.is_whitespace()).filter(|s| !s.is_empty()).map(String::from).collect())
            }
            "ref-diverges" => {
                let v = value.trim();
                let (tool, reason) = v.split_once(char::is_whitespace).unwrap_or((v, ""));
                t.ref_diverges.push((tool.to_string(), reason.trim().to_string()));
            }
            "xfail" => t.xfail = Some(if value.trim().is_empty() { "xfail".into() } else { value.trim().to_string() }),
            "int" => {
                int_given = true;
                t.int_width = match value.trim() {
                    "agnostic" => IntWidth::Agnostic,
                    "16" => IntWidth::Sixteen,
                    other => return Err(err(&format!("loomcc-int must be agnostic or 16, not `{}`", other))),
                }
            }
            "timeout" => t.timeout = Some(value.trim().parse().map_err(|_| err("bad timeout"))?),
            "max-frames" => t.max_frames = Some(value.trim().parse().map_err(|_| err("bad max-frames"))?),
            "extra-sources" => t.extra_sources.extend(value.split_whitespace().map(String::from)),
            "tcc-sources" => t.tcc_sources.extend(value.split_whitespace().map(String::from)),
            "asm-sources" => t.asm_sources.extend(value.split_whitespace().map(String::from)),
            "skip-mode" => t.skip_modes.extend(value.split(|c: char| c == ',' || c.is_whitespace()).filter(|s| !s.is_empty()).map(String::from)),
            "note" | "source" | "licence" | "license" => {}
            other => return Err(err(&format!("unknown directive loomcc-{}", other))),
        }
    }
    let Some(action) = action else { return Ok(None) };
    t.action = action;
    if action == Action::Run && !int_given {
        return Err(format!("{}: run tests must say `loomcc-int: agnostic` or `loomcc-int: 16`", path.display()));
    }
    if have_expect {
        t.expect = Some(expect_lines.join("\n"));
    }
    let sidecar = path.with_extension("expected");
    if sidecar.exists() {
        if t.expect.is_some() {
            return Err(format!("{}: both loomcc-expect lines and a .expected file", path.display()));
        }
        t.expect = Some(std::fs::read_to_string(&sidecar).map_err(|e| format!("{}: {}", sidecar.display(), e))?);
    }
    Ok(Some(t))
}

/// `+N`, `-N`, `N`, `file:N`, `file:+N`.
fn parse_at(s: &str, here: u32) -> Result<DiagLine, String> {
    let (file, num) = match s.rsplit_once(':') {
        Some((f, n)) => (Some(f.to_string()), n),
        None => (None, s),
    };
    let line = if let Some(n) = num.strip_prefix('+') {
        here + n.parse::<u32>().map_err(|_| format!("bad line `{}`", s))?
    } else if let Some(n) = num.strip_prefix('-') {
        here.checked_sub(n.parse::<u32>().map_err(|_| format!("bad line `{}`", s))?).ok_or("line before 1")?
    } else {
        num.parse::<u32>().map_err(|_| format!("bad line `{}`", s))?
    };
    Ok(DiagLine::At(file, line))
}

/// Splits an option string on whitespace, honouring '...' and "..." quoting.
pub fn shell_split(s: &str) -> Result<Vec<String>, String> {
    let mut out = Vec::new();
    let mut cur = String::new();
    let mut have = false;
    let mut chars = s.chars();
    while let Some(c) = chars.next() {
        match c {
            '\'' => {
                have = true;
                loop {
                    match chars.next() {
                        Some('\'') => break,
                        Some(c) => cur.push(c),
                        None => return Err("unterminated '".into()),
                    }
                }
            }
            '"' => {
                have = true;
                loop {
                    match chars.next() {
                        Some('"') => break,
                        Some('\\') => {
                            if let Some(c) = chars.next() {
                                cur.push(c)
                            }
                        }
                        Some(c) => cur.push(c),
                        None => return Err("unterminated \"".into()),
                    }
                }
            }
            c if c.is_whitespace() => {
                if have {
                    out.push(std::mem::take(&mut cur));
                    have = false;
                }
            }
            c => {
                have = true;
                cur.push(c)
            }
        }
    }
    if have {
        out.push(cur);
    }
    Ok(out)
}
