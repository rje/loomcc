//! loomcc-bench: runs the testbed benchmarks through the harness and writes
//! the RESULTS tables.
//!
//!   loomcc-bench run <outdir> [bench ...]     # host, tcc, asm, loomcc
//!   loomcc-bench report <outdir>              # markdown on stdout
//!   loomcc-bench compare <old-outdir> <new-outdir>   # loomcc before/after

use serde_json::Value;
use std::path::{Path, PathBuf};
use std::process::{Command, ExitCode};

fn repo() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("../..")
}

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().skip(1).collect();
    match args.first().map(String::as_str) {
        Some("run") if args.len() >= 2 => {
            let mut cmd = Command::new("nice");
            cmd.args(["-n", "19", "python3"]).arg(repo().join("testbed/harness/run_all.py")).arg(&args[1]);
            cmd.args(["--variants", "host,tcc,asm,loomcc"]).args(&args[2..]);
            match cmd.status() {
                Ok(s) if s.success() => ExitCode::SUCCESS,
                _ => ExitCode::from(1),
            }
        }
        Some("report") if args.len() == 2 => {
            print!("{}", report(Path::new(&args[1])));
            ExitCode::SUCCESS
        }
        Some("compare") if args.len() == 3 => {
            print!("{}", compare(Path::new(&args[1]), Path::new(&args[2])));
            ExitCode::SUCCESS
        }
        _ => {
            eprintln!("usage: loomcc-bench run <outdir> [bench..] | report <outdir> | compare <old> <new>");
            ExitCode::from(2)
        }
    }
}

struct Row {
    name: String,
    kind: String,
    equal: bool,
    v: [Option<Metrics>; 3], // tcc, asm, loomcc
    cstack: Option<u64>,
}

#[derive(Clone, Copy)]
struct Metrics {
    bytes: f64,
    instr: f64,
    clocks: f64,
}

fn metrics(v: &Value) -> Option<Metrics> {
    if v.get("error").is_some() {
        return None;
    }
    Some(Metrics {
        bytes: v["code_bytes"].as_f64()? + v["rodata_bytes"].as_f64().unwrap_or(0.0),
        instr: v["unit_and_helper_instructions"].as_f64()?,
        clocks: v["net_master_clocks"].as_f64()?,
    })
}

fn load(out: &Path) -> Vec<Row> {
    let text = std::fs::read_to_string(out.join("summary.json")).expect("summary.json");
    let s: Value = serde_json::from_str(&text).unwrap();
    let mut rows = Vec::new();
    for (name, r) in s.as_object().unwrap() {
        let kind = if repo().join("testbed/bench").join(name).join("unit.asm").exists() { "pair" } else { "micro" };
        let cstack = cstack_bytes(&out.join(name).join("loomcc/build/unit.asm"));
        rows.push(Row {
            name: name.clone(),
            kind: kind.into(),
            equal: r["equal"].as_bool().unwrap_or(false),
            v: [r.get("tcc").and_then(metrics), r.get("asm").and_then(metrics), r.get("loomcc").and_then(metrics)],
            cstack,
        });
    }
    rows.sort_by(|a, b| (a.kind.as_str() == "micro", &a.name).cmp(&(b.kind.as_str() == "micro", &b.name)));
    rows
}

fn cstack_bytes(asm: &Path) -> Option<u64> {
    let text = std::fs::read_to_string(asm).ok()?;
    let mut in_cstack = false;
    for line in text.lines() {
        if line.contains(".RAMSECTION") && line.contains("cstack") {
            in_cstack = true;
            continue;
        }
        if in_cstack {
            if let Some(n) = line.split("dsb").nth(1) {
                return n.trim().parse().ok();
            }
        }
    }
    Some(0)
}

fn geo(xs: &[f64]) -> f64 {
    if xs.is_empty() {
        return f64::NAN;
    }
    (xs.iter().map(|x| x.ln()).sum::<f64>() / xs.len() as f64).exp()
}

fn fmt_int(x: f64) -> String {
    format!("{}", x.round() as i64)
}

fn report(out: &Path) -> String {
    let rows = load(out);
    let mut s = String::new();
    s.push_str("| bench | kind | equal | tcc bytes | tcc instr | tcc clocks | asm bytes | asm instr | asm clocks | loomcc bytes | loomcc instr | loomcc clocks | loomcc/tcc clocks | loomcc/asm clocks | cstack bytes |\n");
    s.push_str("|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n");
    let (mut vs_tcc, mut vs_asm) = (Vec::new(), Vec::new());
    let (mut b_tcc, mut b_asm, mut i_tcc, mut i_asm) = (Vec::new(), Vec::new(), Vec::new(), Vec::new());
    let mut asm_vs_tcc = Vec::new();
    for r in &rows {
        let cell = |m: &Option<Metrics>| match m {
            Some(m) => format!("{} | {} | {}", fmt_int(m.bytes), fmt_int(m.instr), fmt_int(m.clocks)),
            None => "- | - | -".to_string(),
        };
        let ratio = |a: &Option<Metrics>, b: &Option<Metrics>| match (a, b) {
            (Some(a), Some(b)) => format!("{:.2}", a.clocks / b.clocks),
            _ => "-".into(),
        };
        s.push_str(&format!(
            "| {} | {} | {} | {} | {} | {} | {} | {} | {} |\n",
            r.name,
            r.kind,
            if r.equal { "yes" } else { "**NO**" },
            cell(&r.v[0]),
            cell(&r.v[1]),
            cell(&r.v[2]),
            ratio(&r.v[2], &r.v[0]),
            ratio(&r.v[2], &r.v[1]),
            r.cstack.map_or("-".into(), |c| c.to_string())
        ));
        if let (Some(l), Some(t)) = (&r.v[2], &r.v[0]) {
            vs_tcc.push(l.clocks / t.clocks);
            b_tcc.push(l.bytes / t.bytes);
            i_tcc.push(l.instr / t.instr);
        }
        if let (Some(l), Some(a)) = (&r.v[2], &r.v[1]) {
            vs_asm.push(l.clocks / a.clocks);
            b_asm.push(l.bytes / a.bytes);
            i_asm.push(l.instr / a.instr);
        }
        if let (Some(a), Some(t)) = (&r.v[1], &r.v[0]) {
            asm_vs_tcc.push(a.clocks / t.clocks);
        }
    }
    let equal = rows.iter().filter(|r| r.equal).count();
    s.push_str(&format!(
        "\n{} benchmarks, {} with identical result words in every variant.\n\n\
         Geometric means, loomcc relative to 816-tcc (all benchmarks): clocks {:.2}x, instructions {:.2}x, bytes {:.2}x.\n\
         Geometric means, loomcc relative to hand assembly (the {} pairs): clocks {:.2}x, instructions {:.2}x, bytes {:.2}x.\n\
         For scale, hand assembly relative to 816-tcc: clocks {:.2}x.\n",
        rows.len(),
        equal,
        geo(&vs_tcc),
        geo(&i_tcc),
        geo(&b_tcc),
        vs_asm.len(),
        geo(&vs_asm),
        geo(&i_asm),
        geo(&b_asm),
        geo(&asm_vs_tcc)
    ));
    let total_cstack: u64 = rows.iter().filter_map(|r| r.cstack).sum();
    let max_cstack = rows.iter().filter_map(|r| r.cstack).max().unwrap_or(0);
    s.push_str(&format!("Compiled-stack WRAM: largest single benchmark {} bytes; sum over all {} bytes.\n", max_cstack, total_cstack));
    s
}

fn compare(old: &Path, new: &Path) -> String {
    let a = load(old);
    let b = load(new);
    let mut s = String::from("| bench | loomcc clocks before | after | change |\n|---|---:|---:|---:|\n");
    for r in &b {
        let Some(o) = a.iter().find(|x| x.name == r.name) else { continue };
        if let (Some(x), Some(y)) = (&o.v[2], &r.v[2]) {
            s.push_str(&format!("| {} | {} | {} | {:+.1}% |\n", r.name, fmt_int(x.clocks), fmt_int(y.clocks), 100.0 * (y.clocks / x.clocks - 1.0)));
        }
    }
    s
}
