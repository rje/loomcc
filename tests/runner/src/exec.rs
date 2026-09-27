//! Running tools (always under `nice -n 19`) with a timeout, and parsing
//! their diagnostics.

use crate::directives::{DiagKind, DiagLine, ExpectedDiag};
use regex::Regex;
use std::io::Read;
use std::path::Path;
use std::process::{Command, Stdio};
use std::sync::OnceLock;
use std::time::{Duration, Instant};

#[derive(Debug, Clone)]
pub struct Output {
    /// Exit code; None when killed by a signal or the timeout.
    pub code: Option<i32>,
    pub timed_out: bool,
    pub stdout: String,
    pub stderr: String,
    pub cmdline: String,
}

impl Output {
    pub fn ok(&self) -> bool {
        self.code == Some(0)
    }
    /// A crash: killed by a signal, a Rust panic (101) or a timeout.
    pub fn crashed(&self) -> bool {
        self.timed_out || self.code.is_none() || self.code == Some(101)
    }
    pub fn describe(&self) -> String {
        if self.timed_out {
            "timed out".into()
        } else {
            match self.code {
                Some(c) => format!("exit {}", c),
                None => "killed by a signal".into(),
            }
        }
    }
}

pub fn run(program: &Path, args: &[String], cwd: &Path, timeout: Duration) -> Output {
    let cmdline = format!("{} {}", program.display(), args.join(" "));
    // Compilers and reference tools run in the background QoS band
    // (taskpolicy -b) plus nice -n 19: the machine also runs Loom's ROM
    // suites. The emulator does not: in the background band it gets too
    // little CPU to produce a frame within MesenCore's five-second limit, so
    // it runs under plain nice -n 10. (taskpolicy is macOS-only; elsewhere
    // plain nice.)
    let emulator = program.file_name().map_or(false, |n| n.to_string_lossy().contains("loom-emulator"));
    let mut cmd = if cfg!(target_os = "macos") && !emulator {
        let mut c = Command::new("taskpolicy");
        c.arg("-b").arg("nice");
        c
    } else {
        Command::new("nice")
    };
    cmd.arg("-n").arg(if emulator { "10" } else { "19" }).arg(program).args(args).current_dir(cwd);
    cmd.stdin(Stdio::null()).stdout(Stdio::piped()).stderr(Stdio::piped());
    cmd.env("CARGO_BUILD_JOBS", "2");
    let mut child = match cmd.spawn() {
        Ok(c) => c,
        Err(e) => {
            return Output { code: Some(127), timed_out: false, stdout: String::new(), stderr: format!("cannot run: {}", e), cmdline }
        }
    };
    let mut so = child.stdout.take().unwrap();
    let mut se = child.stderr.take().unwrap();
    let t_out = std::thread::spawn(move || {
        let mut v = Vec::new();
        let _ = so.read_to_end(&mut v);
        v
    });
    let t_err = std::thread::spawn(move || {
        let mut v = Vec::new();
        let _ = se.read_to_end(&mut v);
        v
    });
    let start = Instant::now();
    let mut timed_out = false;
    let status = loop {
        match child.try_wait() {
            Ok(Some(s)) => break Some(s),
            Ok(None) => {
                if start.elapsed() > timeout {
                    let _ = child.kill();
                    timed_out = true;
                    break child.wait().ok();
                }
                std::thread::sleep(Duration::from_millis(5));
            }
            Err(_) => break None,
        }
    };
    let stdout = String::from_utf8_lossy(&t_out.join().unwrap_or_default()).into_owned();
    let stderr = String::from_utf8_lossy(&t_err.join().unwrap_or_default()).into_owned();
    let code = if timed_out { None } else { status.and_then(|s| s.code()) };
    Output { code, timed_out, stdout, stderr, cmdline }
}

#[derive(Debug, Clone)]
pub struct Diag {
    pub file: Option<String>,
    pub line: Option<u32>,
    pub kind: DiagKind,
    pub msg: String,
}

fn diag_re() -> &'static Regex {
    static RE: OnceLock<Regex> = OnceLock::new();
    RE.get_or_init(|| {
        Regex::new(r"^(?:(?P<file>[^:\n]*?):(?P<line>\d+):(?:(?P<col>\d+):)?\s*)?(?:fatal\s+)?(?P<kind>error|warning)\s*:\s*(?P<msg>.*)$").unwrap()
    })
}

/// Parses `file:line[:col]: error|warning: msg` lines (clang, gcc, 816-tcc
/// and loomcc all use this shape), plus unlocated `tool: error: msg`.
pub fn parse_diags(stderr: &str) -> Vec<Diag> {
    let mut out = Vec::new();
    for line in stderr.lines() {
        if let Some(c) = diag_re().captures(line.trim_end()) {
            let kind = if &c["kind"] == "error" { DiagKind::Error } else { DiagKind::Warning };
            let file = c.name("file").map(|m| m.as_str().to_string());
            let line = c.name("line").and_then(|m| m.as_str().parse().ok());
            out.push(Diag { file, line, kind, msg: c["msg"].to_string() });
        } else if let Some(rest) = line.strip_prefix("loomcc: ") {
            // `loomcc: <message>` with no level: count it as an unlocated error.
            if !rest.starts_with("warning") && !rest.starts_with("note") {
                out.push(Diag { file: None, line: None, kind: DiagKind::Error, msg: rest.to_string() });
            }
        }
    }
    out
}

fn same_file(diag_file: &Option<String>, want: &Option<String>, test_file: &str) -> bool {
    let want = want.as_deref().unwrap_or(test_file);
    match diag_file {
        None => false,
        Some(f) => {
            let f = f.trim_start_matches("./");
            let w = want.trim_start_matches("./");
            f == w || f.ends_with(&format!("/{}", w))
        }
    }
}

pub struct DiagCheck {
    pub problems: Vec<String>,
}

/// Matches expected diagnostics against actual ones.
/// `strict`: loomcc mode (errors must be errors, messages must match, no
/// unexpected errors). Otherwise reference mode: any diagnostic at the line
/// satisfies an expected error or warning, messages are not checked, and
/// unexpected warnings are ignored.
pub fn check_diags(expected: &[ExpectedDiag], actual: &[Diag], test_file: &str, strict: bool, no_warnings: bool) -> DiagCheck {
    let mut used = vec![false; actual.len()];
    let mut problems = Vec::new();
    for e in expected {
        let found = actual.iter().enumerate().position(|(i, d)| {
            if used[i] {
                return false;
            }
            if strict && e.kind != DiagKind::Any && d.kind != e.kind {
                return false;
            }
            let loc_ok = match &e.line {
                DiagLine::Anywhere => true,
                DiagLine::At(f, l) => d.line == Some(*l) && same_file(&d.file, f, test_file),
            };
            let msg_ok = !strict || e.pattern.as_ref().map_or(true, |p| p.is_match(&d.msg));
            loc_ok && msg_ok
        });
        match found {
            Some(i) => used[i] = true,
            None => {
                let where_ = match &e.line {
                    DiagLine::Anywhere => "anywhere".to_string(),
                    DiagLine::At(f, l) => format!("{}:{}", f.as_deref().unwrap_or(test_file), l),
                };
                let kind = match e.kind {
                    DiagKind::Error => "error",
                    DiagKind::Warning => "warning",
                    DiagKind::Any => "diagnostic",
                };
                problems.push(format!(
                    "missing {} at {}{} (directive line {})",
                    kind,
                    where_,
                    if strict && !e.pattern_text.is_empty() { format!(" matching /{}/", e.pattern_text) } else { String::new() },
                    e.directive_line
                ));
            }
        }
    }
    // Lines that carry an expected diagnostic: follow-on errors there are
    // tolerated (a cascade), anywhere else they are unexpected.
    let expected_lines: Vec<(Option<String>, u32)> = expected
        .iter()
        .filter_map(|e| match &e.line {
            DiagLine::At(f, l) => Some((f.clone(), *l)),
            DiagLine::Anywhere => None,
        })
        .collect();
    for (i, d) in actual.iter().enumerate() {
        if used[i] {
            continue;
        }
        if expected_lines.iter().any(|(f, l)| d.line == Some(*l) && same_file(&d.file, f, test_file)) {
            continue;
        }
        let unexpected = match d.kind {
            DiagKind::Error => true,
            DiagKind::Warning | DiagKind::Any => no_warnings,
        };
        if unexpected {
            problems.push(format!(
                "unexpected {} at {}:{}: {}",
                if d.kind == DiagKind::Error { "error" } else { "warning" },
                d.file.as_deref().unwrap_or("?"),
                d.line.map(|l| l.to_string()).unwrap_or_else(|| "?".into()),
                d.msg
            ));
        }
    }
    DiagCheck { problems }
}
