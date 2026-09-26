//! Planning and executing one (test, mode) job.
//!
//! loomcc modes: `E` (-E), `syntax` (-fsyntax-only), `S` (-S, then
//! wla-65816), `ir` (--run-ir), `rom` (-S, harness ROM, loom-emulator).
//! Reference modes: `ref:clang`, `ref:clang16`, `ref:tcc`, `ref:host`,
//! `ref:host16`, `ref:tcc-rom`.

use crate::directives::{Action, DiagKind, IntWidth, Test};
use crate::exec::{self, check_diags, parse_diags};
use crate::pptok;
use crate::report::{Outcome, Status};
use crate::rom::{self, RomOutcome};
use crate::Config;
use std::path::{Path, PathBuf};
use std::sync::OnceLock;
use std::time::Duration;

pub struct Job {
    pub id: String,
    pub tier: String,
    pub mode: String,
    pub path: PathBuf,
    pub test: Option<Test>,
    pub broken: Option<String>,
}

impl Job {
    pub fn broken(_i: usize, id: String, e: String) -> Job {
        Job { tier: crate::tier_of(&id), id, mode: "parse".into(), path: PathBuf::new(), test: None, broken: Some(e) }
    }
    pub fn is_ref(&self) -> bool {
        self.mode.starts_with("ref:")
    }
}


pub fn default_refs(t: &Test) -> Vec<&'static str> {
    match t.action {
        Action::Preprocess => vec!["clang", "tcc"],
        Action::Syntax | Action::Compile => vec!["clang16", "tcc"],
        Action::Run => vec!["host", "host16", "tcc-rom"],
    }
}

/// Expands `%ROOT%` (this repository) and `%PVSNESLIB%` in test options.
fn substitute(t: &Test, cfg: &Config) -> Test {
    let mut t = t.clone();
    let pvs = cfg.tools.pvsneslib.as_ref().map(|p| p.display().to_string()).unwrap_or_default();
    let root = cfg.root.display().to_string();
    t.options = t.options.iter().map(|o| o.replace("%PVSNESLIB%", &pvs).replace("%ROOT%", &root)).collect();
    t
}

pub fn plan(_i: usize, id: &str, path: &Path, t: &Test, cfg: &Config) -> Vec<Job> {
    let t = &substitute(t, cfg);
    let mut jobs = Vec::new();
    let mk = |mode: &str| Job {
        id: id.to_string(),
        tier: crate::tier_of(id),
        mode: mode.to_string(),
        path: path.to_path_buf(),
        test: Some(t.clone()),
        broken: None,
    };
    if cfg.run_loomcc {
        let modes: Vec<&str> = match t.action {
            Action::Preprocess => vec!["E"],
            Action::Syntax => vec!["syntax"],
            Action::Compile => vec!["S"],
            Action::Run => vec!["ir", "rom"],
        };
        for m in modes {
            if t.skip_modes.iter().any(|s| s == m) {
                continue;
            }
            if let Some(sel) = &cfg.modes {
                if !sel.iter().any(|s| s == m) {
                    continue;
                }
            }
            jobs.push(mk(m));
        }
    }
    if let Some(sel) = &cfg.refs {
        let wanted: Vec<String> = match &t.refs {
            Some(r) => r.clone(),
            None => default_refs(t).into_iter().map(String::from).collect(),
        };
        for r in wanted {
            if !sel.is_empty() && !sel.iter().any(|s| *s == r) {
                continue;
            }
            if t.skip_modes.iter().any(|s| *s == r) {
                continue;
            }
            if r == "host" && t.int_width == IntWidth::Sixteen {
                continue;
            }
            if (r == "host" || r == "host16") && !t.asm_sources.is_empty() {
                continue;
            }
            jobs.push(mk(&format!("ref:{}", r)));
        }
    }
    jobs
}

static PROBES: OnceLock<Vec<(String, bool)>> = OnceLock::new();

pub fn set_probes(p: Vec<(String, bool)>) {
    let _ = PROBES.set(p);
}

fn probe_ok(mode: &str) -> bool {
    let m = if mode == "rom" { "S" } else { mode };
    PROBES.get().map_or(true, |v| v.iter().find(|(n, _)| n == m).map_or(true, |(_, ok)| *ok))
}

/// Runs loomcc once per needed mode on a trivial program: a mode that
/// cannot compile `int main(void) { return 0; }` is reported UNSUPPORTED
/// for every test instead of failing them all.
pub fn probe_loomcc(cfg: &Config, needed: &[&str]) -> Vec<(String, bool)> {
    let dir = cfg.work.join("_probe");
    let _ = std::fs::create_dir_all(&dir);
    let src = dir.join("probe.c");
    let _ = std::fs::write(&src, "int loomcc_probe_x;\nint main(void) { return 0; }\n");
    let mut out = Vec::new();
    let mut want: Vec<&str> = needed.iter().map(|m| if *m == "rom" { "S" } else { *m }).collect();
    want.sort();
    want.dedup();
    for m in want {
        let t = Duration::from_secs(30);
        let lc = &cfg.tools.loomcc;
        let ok = if !lc.exists() {
            false
        } else {
            match m {
                "E" => {
                    let o = exec::run(lc, &["-E".into(), "probe.c".into()], &dir, t);
                    o.ok() && o.stdout.contains("loomcc_probe_x")
                }
                "syntax" => exec::run(lc, &["-fsyntax-only".into(), "probe.c".into()], &dir, t).ok(),
                "S" => {
                    let _ = std::fs::remove_file(dir.join("probe.asm"));
                    exec::run(lc, &["-S".into(), "probe.c".into(), "-o".into(), "probe.asm".into()], &dir, t).ok()
                        && dir.join("probe.asm").exists()
                }
                "ir" => exec::run(lc, &["--run-ir".into(), "probe.c".into()], &dir, t).ok(),
                _ => true,
            }
        };
        out.push((m.to_string(), ok));
    }
    out
}

fn sanitize(s: &str) -> String {
    s.chars().map(|c| if c.is_ascii_alphanumeric() || c == '.' || c == '-' || c == '_' { c } else { '_' }).collect()
}

pub fn execute(job: &Job, cfg: &Config) -> Outcome {
    let mut o = Outcome { id: job.id.clone(), tier: job.tier.clone(), mode: job.mode.clone(), status: Status::Pass, detail: String::new(), log: String::new() };
    if let Some(e) = &job.broken {
        o.status = Status::Unresolved;
        o.detail = e.clone();
        return o;
    }
    let t = job.test.as_ref().unwrap();
    let work = cfg.work.join(sanitize(&job.id)).join(sanitize(&job.mode));
    let _ = std::fs::remove_dir_all(&work);
    if let Err(e) = std::fs::create_dir_all(&work) {
        o.status = Status::Unresolved;
        o.detail = format!("{}: {}", work.display(), e);
        return o;
    }
    let res = if let Some(tool) = job.mode.strip_prefix("ref:") {
        run_ref(tool, job, t, cfg, &work, &mut o.log)
    } else if !cfg.tools.loomcc.exists() {
        Res::Unsupported(format!("no loomcc binary at {}", cfg.tools.loomcc.display()))
    } else if !probe_ok(&job.mode) {
        Res::Unsupported(format!("loomcc mode {} not working yet (probe failed)", job.mode))
    } else {
        run_loomcc(job, t, cfg, &work, &mut o.log)
    };
    // Expected failures.
    // An --xfail-list entry without a mode covers every loomcc mode; one
    // with a mode (`rom`, `ref:tcc-rom`, ...) covers just that mode.
    let listed = cfg
        .xfail_list
        .iter()
        .find(|(p, m, _)| {
            let path_ok = p == &job.id || job.id.starts_with(&format!("{}/", p.trim_end_matches('/')));
            let mode_ok = match m {
                Some(m) => m == &job.mode,
                None => !job.is_ref(),
            };
            path_ok && mode_ok
        })
        .map(|(_, _, r)| r.clone());
    let xfail_reason: Option<String> = if job.is_ref() {
        let tool = &job.mode[4..];
        t.ref_diverges.iter().find(|(n, _)| n == tool).map(|(_, r)| r.clone()).or(listed)
    } else {
        t.xfail.clone().or(listed)
    };
    match (res, xfail_reason) {
        (Res::Pass, None) => {}
        (Res::Pass, Some(r)) => {
            o.status = Status::Xpass;
            o.detail = format!("expected to fail ({}) but passed", r);
        }
        (Res::Fail(d), None) => {
            o.status = Status::Fail;
            o.detail = d;
        }
        (Res::Fail(d), Some(r)) => {
            o.status = Status::Xfail;
            o.detail = format!("{} [{}]", r, d);
        }
        (Res::Unsupported(d), _) => {
            o.status = Status::Unsupported;
            o.detail = d;
        }
        (Res::Unresolved(d), _) => {
            o.status = Status::Unresolved;
            o.detail = d;
        }
    }
    if cfg.verbose < 2 {
        o.log.clear();
    }
    o
}

enum Res {
    Pass,
    Fail(String),
    Unsupported(String),
    Unresolved(String),
}

fn timeout(t: &Test, default: u64) -> Duration {
    Duration::from_secs(t.timeout.unwrap_or(default))
}

fn file_name(p: &Path) -> String {
    p.file_name().unwrap().to_string_lossy().into_owned()
}

fn expects_errors(t: &Test) -> bool {
    t.diags.iter().any(|d| d.kind == DiagKind::Error)
}

/// Compares `-E` output with the expected text, token by token.
fn compare_tokens(expected: &str, actual: &str) -> Option<String> {
    let e = pptok::tokenize(expected);
    let a = pptok::tokenize(actual);
    if e == a {
        return None;
    }
    let i = e.iter().zip(a.iter()).position(|(x, y)| x != y).unwrap_or(e.len().min(a.len()));
    let ctx = |v: &[String]| -> String {
        let lo = i.saturating_sub(4);
        let hi = (i + 5).min(v.len());
        let mut s = v[lo..hi].join(" ");
        if s.len() > 160 {
            s.truncate(160);
        }
        s
    };
    Some(format!(
        "token {} differs: expected `{}`, got `{}`\n  expected: ... {} ...\n  actual:   ... {} ...",
        i,
        e.get(i).map(|s| s.as_str()).unwrap_or("<end>"),
        a.get(i).map(|s| s.as_str()).unwrap_or("<end>"),
        ctx(&e),
        ctx(&a)
    ))
}

/// The shared checks for a front-end run: crash, diagnostics, exit status.
fn check_frontend(t: &Test, out: &exec::Output, file: &str, strict: bool, tool: &str) -> Vec<String> {
    let mut problems = Vec::new();
    if out.timed_out {
        problems.push(format!("{} timed out", tool));
        return problems;
    }
    if out.code.is_none() || (strict && out.code == Some(101)) {
        problems.push(format!("{} crashed ({}): {}", tool, out.describe(), rom::first_lines(&out.stderr, 3)));
        return problems;
    }
    if out.code == Some(127) && out.stderr.starts_with("cannot run") {
        problems.push(out.stderr.clone());
        return problems;
    }
    let diags = parse_diags(&out.stderr);
    let dc = check_diags(&t.diags, &diags, file, strict, t.no_warnings && strict);
    problems.extend(dc.problems);
    let want_err = expects_errors(t);
    let any_only = t.diags.iter().any(|d| d.kind == DiagKind::Any);
    if any_only && !want_err {
        // An error or a warning satisfies `loomcc-diagnostic`: exit status free,
        // but an unexpected error still shows up in the diagnostic check.
        return problems;
    }
    if strict || !want_err {
        if want_err && out.ok() {
            problems.push(format!("{} exited 0 but errors were expected", tool));
        }
        if !want_err && !out.ok() && problems.is_empty() {
            problems.push(format!("{} failed ({}): {}", tool, out.describe(), rom::first_lines(&out.stderr, 3)));
        }
    }
    problems
}

fn verdict(problems: Vec<String>) -> Res {
    if problems.is_empty() {
        Res::Pass
    } else {
        Res::Fail(problems.join("\n"))
    }
}

fn harness_inc(cfg: &Config) -> String {
    format!("-I{}", cfg.tools.harness_include.display())
}

fn run_loomcc(job: &Job, t: &Test, cfg: &Config, work: &Path, log: &mut String) -> Res {
    let dir = job.path.parent().unwrap();
    let file = file_name(&job.path);
    let lc = &cfg.tools.loomcc;
    match job.mode.as_str() {
        "E" => {
            let mut args = t.options.clone();
            args.push("-E".into());
            args.push(file.clone());
            let out = exec::run(lc, &args, dir, timeout(t, 30));
            rom::log_cmd(log, &out);
            let mut p = check_frontend(t, &out, &file, true, "loomcc");
            if p.is_empty() || !out.crashed() {
                if let Some(exp) = &t.expect {
                    if let Some(d) = compare_tokens(exp, &out.stdout) {
                        p.push(d);
                    }
                }
            }
            verdict(p)
        }
        "syntax" => {
            let mut args = vec![harness_inc(cfg)];
            args.extend(t.options.clone());
            args.push("-fsyntax-only".into());
            args.push(file.clone());
            let out = exec::run(lc, &args, dir, timeout(t, 30));
            rom::log_cmd(log, &out);
            verdict(check_frontend(t, &out, &file, true, "loomcc"))
        }
        "S" => {
            let asm = work.join("out.asm");
            let mut args = vec![harness_inc(cfg)];
            args.extend(t.options.clone());
            args.extend(["-S".into(), file.clone(), "-o".into(), asm.display().to_string()]);
            let out = exec::run(lc, &args, dir, timeout(t, 60));
            rom::log_cmd(log, &out);
            let mut p = check_frontend(t, &out, &file, true, "loomcc");
            if p.is_empty() && !expects_errors(t) {
                if !asm.exists() {
                    p.push("loomcc -S wrote no output".into());
                } else if let Some(wla) = &cfg.tools.wla {
                    if cfg.tools.pvsneslib.is_some() {
                        let _ = rom::write_hdr(&cfg.tools, work);
                        let o = exec::run(
                            wla,
                            &["-d".into(), "-s".into(), "-x".into(), "-o".into(), "out.obj".into(), "out.asm".into()],
                            work,
                            Duration::from_secs(60),
                        );
                        rom::log_cmd(log, &o);
                        if !o.ok() {
                            p.push(format!("wla-65816 rejected loomcc's output: {}", rom::first_lines(&o.stderr, 5)));
                        }
                    }
                }
            }
            verdict(p)
        }
        "ir" => {
            let mut args = vec![harness_inc(cfg)];
            args.extend(t.options.clone());
            args.push("--run-ir".into());
            args.push(file.clone());
            for s in t.extra_sources.iter().chain(t.tcc_sources.iter()) {
                args.push(s.clone());
            }
            let out = exec::run(lc, &args, dir, timeout(t, 60));
            rom::log_cmd(log, &out);
            if out.timed_out {
                return Res::Fail("interpreter timed out".into());
            }
            if out.ok() {
                check_stdout(t, &out.stdout)
            } else {
                Res::Fail(format!("--run-ir: {} {}", out.describe(), rom::first_lines(&out.stderr, 3)))
            }
        }
        "rom" => {
            if !cfg.tools.rom_ready() {
                return Res::Unsupported("wla-65816, wlalink, PVSnesLib or loom-emulator missing".into());
            }
            let mut units = Vec::new();
            let flags = rom_flags(t, cfg);
            let sources: Vec<String> = std::iter::once(file.clone()).chain(t.extra_sources.iter().cloned()).collect();
            for (n, s) in sources.iter().enumerate() {
                let asm = work.join(format!("u{}.asm", n));
                let mut args = flags.clone();
                args.extend(["-S".into(), s.clone(), "-o".into(), asm.display().to_string()]);
                let out = exec::run(lc, &args, dir, timeout(t, 60));
                rom::log_cmd(log, &out);
                if !out.ok() {
                    return Res::Fail(format!("loomcc -S {} failed ({}): {}", s, out.describe(), rom::first_lines(&out.stderr, 3)));
                }
                units.push(asm);
            }
            if let Err(e) = add_tcc_and_asm(t, cfg, dir, work, &flags, &mut units, log) {
                return Res::Unresolved(e);
            }
            if cfg.tools.tcc.is_none() {
                return Res::Unsupported("the ROM harness needs 816-tcc for its stdio".into());
            }
            rom_verdict(rom::link_and_run(&cfg.tools, work, &units, t.max_frames.unwrap_or(300), t.expect_output.as_deref(), log))
        }
        m => Res::Unresolved(format!("unknown mode {}", m)),
    }
}

/// Compares a run's stdout with the test's expected output, if any.
fn check_stdout(t: &Test, stdout: &str) -> Res {
    let Some(exp) = &t.expect_output else { return Res::Pass };
    let got = stdout.as_bytes();
    if got == exp.as_slice() {
        return Res::Pass;
    }
    let at = got.iter().zip(exp.iter()).position(|(a, b)| a != b).unwrap_or(got.len().min(exp.len()));
    let snip = |b: &[u8]| String::from_utf8_lossy(&b[at.min(b.len())..(at + 24).min(b.len())]).replace('\n', "\\n");
    Res::Fail(format!("output differs at byte {} (printed {}, expected {}): got `{}`, expected `{}`", at, got.len(), exp.len(), snip(got), snip(exp)))
}

fn rom_flags(t: &Test, cfg: &Config) -> Vec<String> {
    let mut flags: Vec<String> = rom::ROM_DEFINES.iter().map(|s| s.to_string()).collect();
    flags.push(harness_inc(cfg));
    flags.extend(t.options.clone());
    flags
}

fn add_tcc_and_asm(t: &Test, cfg: &Config, dir: &Path, work: &Path, flags: &[String], units: &mut Vec<PathBuf>, log: &mut String) -> Result<(), String> {
    for (n, s) in t.tcc_sources.iter().enumerate() {
        let asm = work.join(format!("tcc{}.asm", n));
        rom::tcc_compile(&cfg.tools, &dir.join(s), &asm, flags, dir, log)?;
        units.push(asm);
    }
    for s in &t.asm_sources {
        let dst = work.join(Path::new(s).file_name().unwrap());
        std::fs::copy(dir.join(s), &dst).map_err(|e| format!("{}: {}", s, e))?;
        units.push(dst);
    }
    Ok(())
}

fn rom_verdict(r: RomOutcome) -> Res {
    match r {
        RomOutcome::Pass => Res::Pass,
        RomOutcome::Fail(d) => Res::Fail(d),
        RomOutcome::Broken(d) => Res::Unresolved(d),
    }
}

fn run_ref(tool: &str, job: &Job, t: &Test, cfg: &Config, work: &Path, log: &mut String) -> Res {
    let dir = job.path.parent().unwrap();
    let file = file_name(&job.path);
    let tools = &cfg.tools;
    let need = |p: &Option<PathBuf>, what: &str| -> Result<PathBuf, Res> { p.clone().ok_or_else(|| Res::Unsupported(format!("{} not found", what))) };
    macro_rules! need {
        ($p:expr, $w:expr) => {
            match need($p, $w) {
                Ok(v) => v,
                Err(r) => return r,
            }
        };
    }
    match (t.action, tool) {
        (Action::Preprocess, "clang") | (Action::Preprocess, "tcc") => {
            let (prog, mut args) = if tool == "clang" {
                (need!(&tools.clang, "clang"), vec!["-E".to_string(), "-P".into(), "-std=c17".into(), "-pedantic".into(), "-fno-caret-diagnostics".into()])
            } else {
                (need!(&tools.tcc, "816-tcc"), vec!["-E".to_string()])
            };
            args.extend(t.options.clone());
            args.push(file.clone());
            let mut out = exec::run(&prog, &args, dir, timeout(t, 30));
            rom::log_cmd(log, &out);
            // 816-tcc -E aborts (SIGABRT) while exiting after writing complete
            // output; treat that as success when it printed no errors.
            if tool == "tcc" && out.code.is_none() && !out.timed_out && !parse_diags(&out.stderr).iter().any(|d| d.kind == DiagKind::Error) {
                out.code = Some(0);
            }
            let mut p = check_frontend(t, &out, &file, false, tool);
            if !expects_errors(t) && !out.crashed() {
                if let Some(exp) = &t.expect {
                    if let Some(d) = compare_tokens(exp, &pptok::strip_line_markers(&out.stdout)) {
                        p.push(d);
                    }
                }
            }
            verdict(p)
        }
        (Action::Syntax | Action::Compile, "clang16") | (Action::Syntax | Action::Compile, "clang") => {
            let prog = need!(&tools.clang, "clang");
            let mut args = vec!["-std=c17".to_string(), "-pedantic".into(), "-fsyntax-only".into(), "-fno-caret-diagnostics".into()];
            if tool == "clang16" {
                // Plain char is signed for loomcc and 816-tcc; clang's msp430
                // default changed between versions, so pin it.
                args.push("--target=msp430-none-elf".into());
                args.push("-fsigned-char".into());
            }
            args.push(harness_inc(cfg));
            args.extend(t.options.clone());
            args.push(file.clone());
            let out = exec::run(&prog, &args, dir, timeout(t, 30));
            rom::log_cmd(log, &out);
            verdict(check_frontend(t, &out, &file, false, tool))
        }
        (Action::Syntax | Action::Compile, "tcc") => {
            let prog = need!(&tools.tcc, "816-tcc");
            let mut args = vec![harness_inc(cfg)];
            args.extend(tools.tcc_includes());
            args.extend(t.options.clone());
            args.extend(["-c".into(), file.clone(), "-o".into(), work.join("out.ps").display().to_string()]);
            let out = exec::run(&prog, &args, dir, timeout(t, 30));
            rom::log_cmd(log, &out);
            verdict(check_frontend(t, &out, &file, false, tool))
        }
        (Action::Run, "host") => {
            let prog = need!(&tools.clang, "clang");
            let exe = work.join("host.exe");
            let mut args = vec![
                "-std=c17".to_string(),
                "-fsigned-char".into(),
                "-O1".into(),
                "-w".into(),
                // Undefined behaviour traps (no runtime library to load):
                // a test that relies on UB fails on the host.
                "-fsanitize=undefined".into(),
                "-fsanitize-trap=all".into(),
                "-DLOOMCC_TEST_HOST=1".into(),
                harness_inc(cfg),
            ];
            args.extend(t.options.clone());
            args.push(file.clone());
            args.extend(t.extra_sources.iter().cloned());
            args.extend(t.tcc_sources.iter().cloned());
            args.extend(["-o".into(), exe.display().to_string()]);
            let out = exec::run(&prog, &args, dir, timeout(t, 60));
            rom::log_cmd(log, &out);
            if !out.ok() {
                return Res::Fail(format!("host clang failed: {}", rom::first_lines(&out.stderr, 5)));
            }
            let out = exec::run(&exe, &[], dir, timeout(t, 30));
            rom::log_cmd(log, &out);
            if out.ok() {
                check_stdout(t, &out.stdout)
            } else {
                Res::Fail(format!("host run: {} {}{}", out.describe(), rom::first_lines(&out.stdout, 2), rom::first_lines(&out.stderr, 2)))
            }
        }
        (Action::Run, "host16") => {
            let clang = need!(&tools.llvm_clang, "LLVM clang");
            let lli = need!(&tools.lli, "lli");
            let mut lls = Vec::new();
            if t.expect_output.is_some() {
                // The host's variadic printf cannot read msp430's 16-bit int
                // arguments, and lli's interpreter crashes on va_arg, so
                // printing tests have no host16 reference.
                return Res::Unsupported("host16 cannot run tests that print".into());
            }
            let sources: Vec<String> = std::iter::once(file.clone()).chain(t.extra_sources.iter().cloned()).chain(t.tcc_sources.iter().cloned()).collect();
            for (n, s) in sources.iter().enumerate() {
                let ll = work.join(format!("s{}.ll", n));
                let mut args = vec!["--target=msp430-none-elf".to_string(), "-fsigned-char".into(), "-std=c17".into(), "-O0".into(), "-w".into(), "-S".into(), "-emit-llvm".into(), "-DLOOMCC_TEST_HOST=1".into(), harness_inc(cfg)];
                args.extend(t.options.clone());
                args.extend([s.clone(), "-o".into(), ll.display().to_string()]);
                let out = exec::run(&clang, &args, dir, timeout(t, 60));
                rom::log_cmd(log, &out);
                if !out.ok() {
                    return Res::Fail(format!("clang (msp430) failed: {}", rom::first_lines(&out.stderr, 5)));
                }
                lls.push(ll);
            }
            let linked = if lls.len() == 1 {
                lls[0].clone()
            } else {
                let link = need!(&tools.llvm_link, "llvm-link");
                let dst = work.join("linked.ll");
                let mut args: Vec<String> = vec!["-S".into(), "-o".into(), dst.display().to_string()];
                args.extend(lls.iter().map(|p| p.display().to_string()));
                let out = exec::run(&link, &args, work, timeout(t, 60));
                rom::log_cmd(log, &out);
                if !out.ok() {
                    return Res::Fail(format!("llvm-link failed: {}", rom::first_lines(&out.stderr, 5)));
                }
                dst
            };
            // The LLVM interpreter keeps host pointers in memory: widen the
            // msp430 data layout's 16-bit pointers so they survive a store.
            let text = std::fs::read_to_string(&linked).unwrap_or_default().replace("p:16:16", "p:64:64").replace(" optnone", "");
            let widened = work.join("widened.ll");
            let _ = std::fs::write(&widened, text);
            // lli's interpreter zero-extends GEP indices narrower than 32
            // bits; instcombine rewrites them to the (now 64-bit) index type.
            let opt = need!(&tools.llvm_opt, "opt");
            let patched = work.join("host16.ll");
            let out = exec::run(
                &opt,
                &["-S".into(), "-passes=instcombine<no-verify-fixpoint>".into(), widened.display().to_string(), "-o".into(), patched.display().to_string()],
                work,
                timeout(t, 60),
            );
            rom::log_cmd(log, &out);
            if !out.ok() {
                return Res::Unresolved(format!("opt failed: {}", rom::first_lines(&out.stderr, 3)));
            }
            // ... and the interpreter has no `freeze`: a same-type bitcast is equivalent here.
            let freeze = regex::Regex::new(r"= freeze (\S+) (.+)$").unwrap();
            let text: String = std::fs::read_to_string(&patched)
                .unwrap_or_default()
                .lines()
                .map(|l| freeze.replace(l, "= bitcast $1 $2 to $1").into_owned() + "\n")
                .collect();
            let _ = std::fs::write(&patched, text);
            let out = exec::run(&lli, &["-force-interpreter".into(), patched.display().to_string()], work, timeout(t, 60));
            rom::log_cmd(log, &out);
            if out.ok() {
                check_stdout(t, &out.stdout)
            } else {
                let check = out.stdout.lines().find(|l| l.starts_with("CHECK failed")).map(String::from);
                Res::Fail(format!("lli: {}: {}", out.describe(), check.unwrap_or_else(|| rom::first_lines(&out.stderr, 1))))
            }
        }
        (Action::Run, "tcc-rom") => {
            if tools.tcc.is_none() || !tools.rom_ready() {
                return Res::Unsupported("816-tcc, wla, PVSnesLib or loom-emulator missing".into());
            }
            let flags = rom_flags(t, cfg);
            let mut units = Vec::new();
            let sources: Vec<String> = std::iter::once(file.clone()).chain(t.extra_sources.iter().cloned()).collect();
            for (n, s) in sources.iter().enumerate() {
                let asm = work.join(format!("u{}.asm", n));
                if let Err(e) = rom::tcc_compile(tools, &dir.join(s), &asm, &flags, dir, log) {
                    return Res::Fail(e);
                }
                units.push(asm);
            }
            if let Err(e) = add_tcc_and_asm(t, cfg, dir, work, &flags, &mut units, log) {
                return Res::Fail(e);
            }
            match rom::link_and_run(tools, work, &units, t.max_frames.unwrap_or(300), t.expect_output.as_deref(), log) {
                RomOutcome::Pass => Res::Pass,
                RomOutcome::Fail(d) => Res::Fail(d),
                RomOutcome::Broken(d) => Res::Unresolved(d),
            }
        }
        (a, tool) => Res::Unresolved(format!("reference `{}` does not apply to a {} test", tool, a.name())),
    }
}
