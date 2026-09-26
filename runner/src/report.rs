//! Results, live output and the per-tier summary.

use std::collections::BTreeMap;
use std::io::Write;
use std::path::Path;

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum Status {
    Pass,
    Fail,
    Xfail,
    Xpass,
    Unsupported,
    Unresolved,
}

impl Status {
    pub fn name(self) -> &'static str {
        match self {
            Status::Pass => "PASS",
            Status::Fail => "FAIL",
            Status::Xfail => "XFAIL",
            Status::Xpass => "XPASS",
            Status::Unsupported => "UNSUPPORTED",
            Status::Unresolved => "UNRESOLVED",
        }
    }
    pub fn bad(self) -> bool {
        matches!(self, Status::Fail | Status::Xpass | Status::Unresolved)
    }
    pub const ALL: [Status; 6] =
        [Status::Pass, Status::Fail, Status::Xfail, Status::Xpass, Status::Unsupported, Status::Unresolved];
}

#[derive(Clone, Debug)]
pub struct Outcome {
    pub id: String,
    pub tier: String,
    /// `E`, `syntax`, `S`, `ir`, `rom`, or `ref:<tool>`.
    pub mode: String,
    pub status: Status,
    pub detail: String,
    /// Command lines and tool output, printed with -vv.
    pub log: String,
}

impl Outcome {
    pub fn is_ref(&self) -> bool {
        self.mode.starts_with("ref:")
    }
}

pub fn print_live(o: &Outcome, verbose: u8) {
    let show = verbose >= 1 || o.status.bad();
    if !show {
        return;
    }
    let mut s = format!("{}: {} [{}]", o.status.name(), o.id, o.mode);
    if !o.detail.is_empty() && (o.status != Status::Pass || verbose >= 1) {
        s.push_str(": ");
        s.push_str(&o.detail.replace('\n', "\n    "));
    }
    if verbose >= 2 && o.status.bad() && !o.log.is_empty() {
        s.push_str("\n    ");
        s.push_str(&o.log.trim_end().replace('\n', "\n    "));
    }
    let stdout = std::io::stdout();
    let mut lock = stdout.lock();
    let _ = writeln!(lock, "{}", s);
}

/// Prints the per-tier table and the summary line; true when anything is bad.
pub fn summary(results: &[Outcome], _planned: usize) -> bool {
    let mut rows: BTreeMap<String, BTreeMap<Status, usize>> = BTreeMap::new();
    for r in results {
        let key = if r.is_ref() { format!("{} refs", r.tier) } else { r.tier.clone() };
        *rows.entry(key).or_default().entry(r.status).or_default() += 1;
    }
    println!();
    println!("{:<16} {:>6} {:>6} {:>6} {:>6} {:>6} {:>6} {:>6}", "tier", "total", "PASS", "FAIL", "XFAIL", "XPASS", "UNSUP", "UNRES");
    let mut tot: BTreeMap<Status, usize> = BTreeMap::new();
    for (k, m) in &rows {
        let n: usize = m.values().sum();
        let g = |s: Status| *m.get(&s).unwrap_or(&0);
        println!(
            "{:<16} {:>6} {:>6} {:>6} {:>6} {:>6} {:>6} {:>6}",
            k,
            n,
            g(Status::Pass),
            g(Status::Fail),
            g(Status::Xfail),
            g(Status::Xpass),
            g(Status::Unsupported),
            g(Status::Unresolved)
        );
        for (s, c) in m {
            *tot.entry(*s).or_default() += c;
        }
    }
    let n: usize = tot.values().sum();
    let parts: Vec<String> =
        Status::ALL.iter().map(|s| format!("{} {}", tot.get(s).copied().unwrap_or(0), s.name())).collect();
    println!("loomcc-tests: {} results: {}", n, parts.join(", "));
    results.iter().any(|r| r.status.bad())
}

fn esc(s: &str) -> String {
    let mut o = String::new();
    for c in s.chars() {
        match c {
            '"' => o.push_str("\\\""),
            '\\' => o.push_str("\\\\"),
            '\n' => o.push_str("\\n"),
            '\t' => o.push_str("\\t"),
            c if (c as u32) < 0x20 => o.push_str(&format!("\\u{:04x}", c as u32)),
            c => o.push(c),
        }
    }
    o
}

pub fn write_json(path: &Path, results: &[Outcome]) -> std::io::Result<()> {
    let mut f = std::fs::File::create(path)?;
    for r in results {
        writeln!(
            f,
            "{{\"test\":\"{}\",\"tier\":\"{}\",\"mode\":\"{}\",\"status\":\"{}\",\"detail\":\"{}\"}}",
            esc(&r.id),
            esc(&r.tier),
            esc(&r.mode),
            r.status.name(),
            esc(&r.detail)
        )?;
    }
    Ok(())
}
