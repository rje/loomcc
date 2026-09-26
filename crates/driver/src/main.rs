//! loomcc: the command-line driver.

use loomcc_pp::{Define, Options, Preprocessor};
use std::path::PathBuf;
use std::process::ExitCode;

struct Args {
    inputs: Vec<PathBuf>,
    pp: Options,
    mode: Mode,
    output: Option<PathBuf>,
}

#[derive(PartialEq)]
enum Mode {
    Preprocess,
    Tokens,
}

fn parse_args() -> Result<Args, String> {
    let mut args = Args { inputs: Vec::new(), pp: Options { target_macros: true, ..Default::default() }, mode: Mode::Preprocess, output: None };
    let mut it = std::env::args().skip(1);
    while let Some(a) = it.next() {
        let mut value = |flag: &str, rest: &str| -> Result<String, String> {
            if !rest.is_empty() {
                Ok(rest.to_string())
            } else {
                it.next().ok_or_else(|| format!("{} needs a value", flag))
            }
        };
        if let Some(rest) = a.strip_prefix("-I") {
            args.pp.include_dirs.push(value("-I", rest)?.into());
        } else if let Some(rest) = a.strip_prefix("-iquote") {
            args.pp.quote_dirs.push(value("-iquote", rest)?.into());
        } else if let Some(rest) = a.strip_prefix("-isystem") {
            args.pp.system_dirs.push(value("-isystem", rest)?.into());
        } else if let Some(rest) = a.strip_prefix("-D") {
            let d = value("-D", rest)?;
            match d.split_once('=') {
                Some((n, v)) => args.pp.defines.push(Define::Set(n.into(), Some(v.into()))),
                None => args.pp.defines.push(Define::Set(d, None)),
            }
        } else if let Some(rest) = a.strip_prefix("-U") {
            args.pp.defines.push(Define::Unset(value("-U", rest)?));
        } else if a == "-E" {
            args.mode = Mode::Preprocess;
        } else if a == "--tokens" {
            args.mode = Mode::Tokens;
        } else if a == "--no-target-macros" {
            args.pp.target_macros = false;
        } else if a == "-o" {
            args.output = Some(it.next().ok_or("-o needs a value")?.into());
        } else if a.starts_with('-') {
            return Err(format!("unknown option {}", a));
        } else {
            args.inputs.push(a.into());
        }
    }
    if args.inputs.is_empty() {
        return Err("no input files".into());
    }
    Ok(args)
}

fn main() -> ExitCode {
    let args = match parse_args() {
        Ok(a) => a,
        Err(e) => {
            eprintln!("loomcc: {}", e);
            return ExitCode::from(2);
        }
    };
    let mut failed = false;
    let mut out = String::new();
    for input in &args.inputs {
        let mut pp = Preprocessor::new(args.pp.clone());
        let toks = match pp.run_file(input) {
            Ok(t) => t,
            Err(e) => {
                eprintln!("loomcc: {}", e);
                failed = true;
                continue;
            }
        };
        for d in &pp.diags {
            eprintln!("{}", pp.sources.render(d));
        }
        failed |= pp.has_errors();
        match args.mode {
            Mode::Preprocess => out.push_str(&loomcc_pp::print::print_tokens(&toks)),
            Mode::Tokens => {
                for t in &toks {
                    if !t.is_eof() {
                        out.push_str(&t.text);
                        out.push('\n');
                    }
                }
            }
        }
    }
    match &args.output {
        Some(p) => {
            if let Err(e) = std::fs::write(p, out) {
                eprintln!("loomcc: {}: {}", p.display(), e);
                return ExitCode::from(1);
            }
        }
        None => print!("{}", out),
    }
    if failed {
        ExitCode::from(1)
    } else {
        ExitCode::SUCCESS
    }
}
