//! loomcc-tests: runs the loomcc torture suite against a loomcc binary and,
//! optionally, against the reference tools (clang, clang for a 16-bit-int
//! target, 816-tcc, and ROMs run in loom-emulator).
//!
//! See README.md for the command line and the test format.

mod directives;
mod exec;
mod modes;
mod pptok;
mod report;
mod rom;
mod tools;

use directives::Test;
use report::{Outcome, Status};
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex};

const USAGE: &str = "\
usage: loomcc-tests [options] [test paths or directories...]

  --loomcc PATH        loomcc binary (default: $LOOMCC, else
                       ../loomcc/target/debug/loomcc beside this repo)
  --tier LIST          tiers to run, comma separated: t1,t2,...,t7 or all (default all)
  --filter TEXT        only tests whose path contains TEXT
  --refs [LIST]        also run the reference tools (default list: the
                       action's defaults; or e.g. clang,tcc,clang16,host,host16,tcc-rom)
  --refs-only          run only the reference tools (validates the suite itself)
  --no-loomcc          same as --refs-only
  --modes LIST         loomcc modes to run: E,syntax,S,ir,rom (default: all that apply)
  --xfail-list FILE    extra expected failures: lines `path [mode] # reason`
  -j N                 parallel jobs (default 2)
  -v                   print every result, not only the unexpected ones
  -vv                  also print command lines and tool output for failures
  --json FILE          write every result as JSON lines
  --work DIR           scratch directory (default $TMPDIR/loomcc-tests-<pid>)
  --keep               keep the scratch directory
  --list               list the tests and their modes, run nothing
  --pvsneslib DIR      PVSnesLib root (816-tcc, wla-65816, crt0)
  --emulator PATH      loom-emulator binary
  --llvm-bin DIR       LLVM bin dir with clang and lli (default /opt/homebrew/opt/llvm/bin)
  --clang PATH         host clang (default clang)

exit status: 0 when no result is FAIL, XPASS or UNRESOLVED; 1 otherwise; 2 usage.
";

pub struct Config {
    pub root: PathBuf,
    pub tools: tools::Tools,
    pub run_loomcc: bool,
    pub refs: Option<Vec<String>>,
    pub modes: Option<Vec<String>>,
    pub xfail_list: Vec<(String, Option<String>, String)>,
    pub verbose: u8,
    pub work: PathBuf,
}

fn main() {
    std::process::exit(match real_main() {
        Ok(code) => code,
        Err(e) => {
            eprintln!("loomcc-tests: {}", e);
            2
        }
    })
}

fn repo_root() -> PathBuf {
    // runner/ lives one level below the repo root.
    let exe_guess = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("..");
    exe_guess.canonicalize().unwrap_or(exe_guess)
}

fn real_main() -> Result<i32, String> {
    let root = repo_root();
    let mut args = std::env::args().skip(1).peekable();
    let mut loomcc: Option<PathBuf> = std::env::var_os("LOOMCC").map(PathBuf::from);
    let mut tiers: Option<Vec<String>> = None;
    let mut filter: Option<String> = None;
    let mut refs: Option<Vec<String>> = None;
    let mut run_loomcc = true;
    let mut modes = None;
    let mut xfail_file = None;
    let mut jobs = 2usize;
    let mut verbose = 0u8;
    let mut json = None;
    let mut work = None;
    let mut keep = false;
    let mut list = false;
    let mut paths: Vec<PathBuf> = Vec::new();
    let mut tool_over = tools::Overrides::default();
    while let Some(a) = args.next() {
        let mut val = |name: &str| args.next().ok_or_else(|| format!("{} needs a value", name));
        match a.as_str() {
            "-h" | "--help" => {
                print!("{}", USAGE);
                return Ok(0);
            }
            "--loomcc" => loomcc = Some(val("--loomcc")?.into()),
            "--tier" | "--tiers" => {
                let v = val("--tier")?;
                if v != "all" {
                    tiers = Some(v.split(',').map(|s| s.trim().to_lowercase()).collect());
                }
            }
            "--filter" => filter = Some(val("--filter")?),
            "--refs" => {
                let list = match args.peek() {
                    Some(n) if !n.starts_with('-') && !Path::new(n).exists() => {
                        let n = args.next().unwrap();
                        n.split(',').map(String::from).collect()
                    }
                    _ => Vec::new(),
                };
                refs = Some(list);
            }
            "--refs-only" | "--no-loomcc" => {
                run_loomcc = false;
                if refs.is_none() {
                    refs = Some(Vec::new());
                }
            }
            "--modes" => modes = Some(val("--modes")?.split(',').map(String::from).collect()),
            "--xfail-list" => xfail_file = Some(PathBuf::from(val("--xfail-list")?)),
            "-j" => jobs = val("-j")?.parse().map_err(|_| "-j needs a number")?,
            "-v" => verbose = verbose.max(1),
            "-vv" => verbose = 2,
            "--json" => json = Some(PathBuf::from(val("--json")?)),
            "--work" => work = Some(PathBuf::from(val("--work")?)),
            "--keep" => keep = true,
            "--list" => list = true,
            "--pvsneslib" => tool_over.pvsneslib = Some(val("--pvsneslib")?.into()),
            "--emulator" => tool_over.emulator = Some(val("--emulator")?.into()),
            "--llvm-bin" => tool_over.llvm_bin = Some(val("--llvm-bin")?.into()),
            "--clang" => tool_over.clang = Some(val("--clang")?.into()),
            s if s.starts_with("-j") => jobs = s[2..].parse().map_err(|_| "-j needs a number")?,
            s if s.starts_with('-') => return Err(format!("unknown option {}\n{}", s, USAGE)),
            _ => paths.push(a.into()),
        }
    }
    let jobs = jobs.max(1);
    if loomcc.is_none() {
        let guess = root.join("../loomcc/target/debug/loomcc");
        loomcc = Some(guess.canonicalize().unwrap_or(guess));
    }
    let loomcc = loomcc.unwrap();
    let loomcc = if loomcc.is_relative() { std::env::current_dir().unwrap().join(loomcc) } else { loomcc };

    // Discover tests.
    let tests_dir = root.join("tests");
    let mut files = Vec::new();
    if paths.is_empty() {
        walk(&tests_dir, &mut files);
    } else {
        for p in &paths {
            let p = if p.is_relative() { std::env::current_dir().unwrap().join(p) } else { p.clone() };
            let p = p.canonicalize().map_err(|e| format!("{}: {}", p.display(), e))?;
            if p.is_dir() {
                walk(&p, &mut files);
            } else {
                files.push(p);
            }
        }
    }
    files.sort();
    let mut tests: Vec<(String, PathBuf, Result<Test, String>)> = Vec::new();
    for f in files {
        // Ids are relative to tests/ (tier = first component); external
        // suites under external/fetched/<suite>/ get the tier `ext-<suite>`.
        let id = match f.strip_prefix(&tests_dir) {
            Ok(rel) => rel.to_string_lossy().into_owned(),
            Err(_) => match f.strip_prefix(root.join("external/fetched")) {
                Ok(rel) => format!("ext-{}", rel.to_string_lossy()),
                Err(_) => f.to_string_lossy().into_owned(),
            },
        };
        let tier = tier_of(&id);
        if let Some(ts) = &tiers {
            if !ts.iter().any(|t| tier.starts_with(t.as_str())) {
                continue;
            }
        }
        if let Some(flt) = &filter {
            if !id.contains(flt.as_str()) {
                continue;
            }
        }
        let text = match std::fs::read(&f) {
            Ok(b) => String::from_utf8_lossy(&b).into_owned(),
            Err(e) => {
                tests.push((id, f, Err(e.to_string())));
                continue;
            }
        };
        match directives::parse(&f, &text) {
            Ok(Some(t)) => tests.push((id, f, Ok(t))),
            Ok(None) => {}
            Err(e) => tests.push((id, f, Err(e))),
        }
    }

    let work = work.unwrap_or_else(|| std::env::temp_dir().join(format!("loomcc-tests-{}", std::process::id())));
    std::fs::create_dir_all(&work).map_err(|e| format!("{}: {}", work.display(), e))?;
    let work = work.canonicalize().unwrap();
    let xfail_list = match &xfail_file {
        Some(p) => parse_xfail_list(p)?,
        None => Vec::new(),
    };
    let tools = tools::Tools::discover(&root, loomcc, tool_over);
    let cfg = Arc::new(Config { root: root.clone(), tools, run_loomcc, refs, modes, xfail_list, verbose, work: work.clone() });

    // Plan the jobs: one per (test, mode).
    let mut jobs_list: Vec<modes::Job> = Vec::new();
    for (i, (id, path, t)) in tests.iter().enumerate() {
        match t {
            Err(e) => jobs_list.push(modes::Job::broken(i, id.clone(), e.clone())),
            Ok(t) => jobs_list.extend(modes::plan(i, id, path, t, &cfg)),
        }
    }
    if list {
        for j in &jobs_list {
            println!("{} [{}]", j.id, j.mode);
        }
        return Ok(0);
    }

    // Probe which loomcc modes work at all.
    if cfg.run_loomcc {
        let needed: Vec<&str> = {
            let mut v: Vec<&str> = jobs_list.iter().filter(|j| !j.is_ref()).map(|j| j.mode.as_str()).collect();
            v.sort();
            v.dedup();
            v
        };
        let probes = modes::probe_loomcc(&cfg, &needed);
        let line: Vec<String> = probes.iter().map(|(m, ok)| format!("{} {}", m, if *ok { "yes" } else { "no" })).collect();
        println!("loomcc: {}", cfg.tools.loomcc.display());
        println!("loomcc modes: {}", if line.is_empty() { "none needed".into() } else { line.join(", ") });
        modes::set_probes(probes);
    }
    println!("reference tools: {}", cfg.tools.describe());

    let total = jobs_list.len();
    let queue = Arc::new(Mutex::new(jobs_list.into_iter().rev().collect::<Vec<_>>()));
    let results: Arc<Mutex<Vec<Outcome>>> = Arc::new(Mutex::new(Vec::new()));
    let mut handles = Vec::new();
    for _ in 0..jobs {
        let q = queue.clone();
        let r = results.clone();
        let cfg = cfg.clone();
        handles.push(std::thread::spawn(move || loop {
            let job = { q.lock().unwrap().pop() };
            let Some(job) = job else { break };
            let out = modes::execute(&job, &cfg);
            report::print_live(&out, cfg.verbose);
            r.lock().unwrap().push(out);
        }));
    }
    for h in handles {
        let _ = h.join();
    }
    let mut results = Arc::try_unwrap(results).map_err(|_| "threads")?.into_inner().unwrap();
    results.sort_by(|a, b| (a.tier.clone(), a.id.clone(), a.mode.clone()).cmp(&(b.tier.clone(), b.id.clone(), b.mode.clone())));
    if let Some(j) = json {
        report::write_json(&j, &results).map_err(|e| format!("{}: {}", j.display(), e))?;
    }
    let bad = report::summary(&results, total);
    if !keep {
        let _ = std::fs::remove_dir_all(&work);
    } else {
        println!("scratch kept in {}", work.display());
    }
    Ok(if bad { 1 } else { 0 })
}

pub fn tier_of(id: &str) -> String {
    id.split('/').next().unwrap_or("").to_string()
}

fn walk(dir: &Path, out: &mut Vec<PathBuf>) {
    let Ok(rd) = std::fs::read_dir(dir) else { return };
    for e in rd.flatten() {
        let p = e.path();
        if p.is_dir() {
            walk(&p, out);
        } else if p.extension().map_or(false, |x| x == "c") {
            out.push(p);
        }
    }
}

fn parse_xfail_list(p: &Path) -> Result<Vec<(String, Option<String>, String)>, String> {
    let text = std::fs::read_to_string(p).map_err(|e| format!("{}: {}", p.display(), e))?;
    let mut v = Vec::new();
    for line in text.lines() {
        let (body, reason) = line.split_once('#').unwrap_or((line, ""));
        let mut it = body.split_whitespace();
        let Some(path) = it.next() else { continue };
        let mode = it.next().map(String::from);
        v.push((path.to_string(), mode, reason.trim().to_string()));
    }
    Ok(v)
}

pub type Counts = BTreeMap<Status, usize>;
