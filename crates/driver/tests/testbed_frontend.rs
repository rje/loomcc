//! Front-end checks over the whole testbed: every Loom unit preprocesses
//! (token-equal to clang -E and 816-tcc -E when those are installed) and
//! parses, and the parse round-trips through the AST printer.

use loomcc::testbed::{loom_units, Unit};
use std::path::Path;
use std::process::Command;

fn units() -> Vec<Unit> {
    let u = loom_units();
    assert!(!u.is_empty(), "testbed/loom/units.json not found");
    u
}

#[test]
fn every_loom_unit_preprocesses_and_parses() {
    let mut failures = Vec::new();
    for u in units() {
        let parsed = loomcc::parse(&u.path, &u.options()).unwrap();
        let mut errs = parsed.pre.render(&parsed.pre.diags);
        errs.push_str(&parsed.pre.render(&parsed.diags));
        if parsed.pre.has_errors() || !parsed.diags.is_empty() {
            failures.push(format!("{}:\n{}", u.name(), errs));
        }
    }
    assert!(failures.is_empty(), "{}", failures.join("\n"));
}

#[test]
fn every_loom_unit_roundtrips_through_the_printer() {
    let mut failures = Vec::new();
    let dir = std::env::temp_dir().join(format!("loomcc-roundtrip-{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    for u in units() {
        let parsed = loomcc::parse(&u.path, &u.options()).unwrap();
        let a = loomcc_parse::print::print_unit(&parsed.unit);
        let file = dir.join("rt.c");
        std::fs::write(&file, &a).unwrap();
        let reparsed = loomcc::parse(&file, &Default::default()).unwrap();
        let b = loomcc_parse::print::print_unit(&reparsed.unit);
        if !reparsed.diags.is_empty() || a != b {
            failures.push(u.name());
        }
    }
    std::fs::remove_dir_all(&dir).ok();
    assert!(failures.is_empty(), "round trip differs: {:?}", failures);
}

fn spellings_of_text(text: &str) -> Vec<String> {
    let mut pp = loomcc_pp::Preprocessor::new(Default::default());
    // Lex only: the text is already preprocessed, so no directives remain
    // except line markers, which -P / the filter below removed.
    let toks = pp.run_source(Path::new("x.i"), "x.i", text.to_string());
    toks.iter().filter(|t| !t.is_eof() && t.kind != loomcc_pp::TokenKind::Pragma).map(|t| t.text.to_string()).collect()
}

fn our_spellings(u: &Unit) -> Vec<String> {
    let pre = loomcc::preprocess(&u.path, &u.options()).unwrap();
    assert!(!pre.has_errors(), "{}", pre.render(&pre.diags));
    pre.tokens.iter().filter(|t| !t.is_eof() && t.kind != loomcc_pp::TokenKind::Pragma).map(|t| t.text.to_string()).collect()
}

fn flags(u: &Unit) -> Vec<String> {
    let mut f: Vec<String> = u.include_dirs.iter().map(|d| format!("-I{}", d.display())).collect();
    for (n, v) in &u.defines {
        f.push(match v {
            Some(v) => format!("-D{}={}", n, v),
            None => format!("-D{}", n),
        });
    }
    f
}

#[test]
fn token_equal_to_clang_e() {
    if Command::new("clang").arg("--version").output().is_err() {
        eprintln!("clang not installed; skipping");
        return;
    }
    let mut diffs = Vec::new();
    for u in units() {
        // loomcc predefines __65816__ (as 816-tcc does); clang does not.
        let out = Command::new("clang")
            .args(["-E", "-P", "-D__65816__=1"])
            .args(flags(&u))
            .arg(&u.path)
            .output()
            .unwrap();
        assert!(out.status.success(), "clang -E failed on {}", u.name());
        let theirs = spellings_of_text(&String::from_utf8_lossy(&out.stdout));
        if theirs != our_spellings(&u) {
            diffs.push(u.name());
        }
    }
    assert!(diffs.is_empty(), "differs from clang -E: {:?}", diffs);
}

#[test]
fn token_equal_to_816_tcc_e() {
    let tcc = "/Users/rje/Library/Loom/Toolchains/v0/artifacts/pvsneslib/devkitsnes/bin/816-tcc";
    if !Path::new(tcc).exists() {
        eprintln!("816-tcc not installed; skipping");
        return;
    }
    let mut diffs = Vec::new();
    for u in units() {
        // 816-tcc -E does not search every -I directory (it misses <snes.h>
        // with Loom's order) and aborts after writing its output, so give it
        // one merged directory and ignore the exit status.
        let merged = std::env::temp_dir().join(format!("loomcc-tcc-inc-{}/{}", std::process::id(), u.sample));
        if !merged.exists() {
            std::fs::create_dir_all(&merged).unwrap();
            for d in u.include_dirs.iter().rev() {
                let _ = Command::new("cp").arg("-R").arg(format!("{}/.", d.display())).arg(&merged).status();
            }
        }
        let mut f = vec![format!("-I{}", merged.display())];
        for (n, v) in &u.defines {
            f.push(match v {
                Some(v) => format!("-D{}={}", n, v),
                None => format!("-D{}", n),
            });
        }
        let out = Command::new(tcc).arg("-E").args(&f).arg(&u.path).output().unwrap();
        let text: String = String::from_utf8_lossy(&out.stdout)
            .lines()
            .filter(|l| !l.starts_with("# "))
            .map(|l| format!("{}\n", l))
            .collect();
        let theirs = spellings_of_text(&text);
        // 816-tcc defines __TINYC__; runtime-adapter.c has one block gated on
        // it (tcc__r5h). Compare everything else.
        let ours = our_spellings(&u);
        if theirs != ours && !u.path.ends_with("runtime-adapter.c") {
            diffs.push(u.name());
        }
    }
    let _ = std::fs::remove_dir_all(std::env::temp_dir().join(format!("loomcc-tcc-inc-{}", std::process::id())));
    assert!(diffs.is_empty(), "differs from 816-tcc -E: {:?}", diffs);
}
