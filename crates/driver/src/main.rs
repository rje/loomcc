//! loomcc: the command-line driver.

use loomcc_pp::{Define, Options, Preprocessor};
use std::path::PathBuf;
use std::process::ExitCode;

struct Args {
    inputs: Vec<PathBuf>,
    pp: Options,
    mode: Mode,
    output: Option<PathBuf>,
    nostdinc: bool,
    opt: u8,
}

#[derive(PartialEq)]
enum Mode {
    Preprocess,
    Tokens,
    /// Parse (and, once sema exists, type-check) only.
    SyntaxOnly,
    /// Parse and print the AST back as C.
    PrintAst,
    /// Whole program to IR, printed.
    EmitIr,
    /// Whole program to WLA-DX assembly.
    Assembly,
    /// Whole program to IR, then run `main` in the interpreter.
    Interpret,
}

fn parse_args() -> Result<Args, String> {
    let mut args = Args { inputs: Vec::new(), pp: Options { target_macros: true, ..Default::default() }, mode: Mode::Preprocess, output: None, nostdinc: false, opt: 2 };
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
        } else if let Some(l) = a.strip_prefix("-O") {
            args.opt = match l {
                "" | "1" => 1,
                "0" => 0,
                _ => 2,
            };
        } else if a == "-fsyntax-only" {
            args.mode = Mode::SyntaxOnly;
        } else if a == "--emit-ir" {
            args.mode = Mode::EmitIr;
        } else if a == "-S" {
            args.mode = Mode::Assembly;
        } else if a == "--interpret" || a == "--run-ir" {
            args.mode = Mode::Interpret;
        } else if a == "--print-ast" {
            args.mode = Mode::PrintAst;
        } else if a == "--tokens" {
            args.mode = Mode::Tokens;
        } else if a == "-nostdinc" {
            args.nostdinc = true;
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
    if !args.nostdinc {
        args.pp.implementation_dirs.push(loomcc::builtin_include_dir());
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
    if matches!(args.mode, Mode::EmitIr | Mode::Assembly | Mode::Interpret) {
        let tag = args
            .output
            .as_ref()
            .or(args.inputs.first())
            .and_then(|p| p.file_stem())
            .map(|s| s.to_string_lossy().to_string())
            .unwrap_or_else(|| "unit".into());
        let prefix = if args.mode == Mode::Assembly { format!("{}_", tag) } else { String::new() };
        let c = loomcc::compile_ir_prefixed(&args.inputs, &args.pp, &prefix, args.opt);
        eprint!("{}", c.messages);
        if c.failed {
            return ExitCode::from(1);
        }
        let m = c.module.unwrap();
        match args.mode {
            Mode::EmitIr => out = loomcc_ir::print_module(&m),
            Mode::Interpret => {
                let mut mc = loomcc_ir::interp::Machine::new(&m);
                let r = mc.run_main("main");
                print!("{}", r.out);
                return match r.exit {
                    Ok(code) => ExitCode::from((code & 0xff) as u8),
                    Err(e) => {
                        eprintln!("loomcc: interpreter: {}", e);
                        ExitCode::from(125)
                    }
                };
            }
            _ => {
                let tag = args
                    .output
                    .as_ref()
                    .or(args.inputs.first())
                    .and_then(|p| p.file_stem())
                    .map(|s| s.to_string_lossy().to_string())
                    .unwrap_or_else(|| "unit".into());
                let o = loomcc_w65816::compile_module(&m, &loomcc_w65816::Options { tag, ..Default::default() });
                for e in &o.errors {
                    eprintln!("loomcc: error: {}", e);
                }
                if !o.errors.is_empty() {
                    return ExitCode::from(1);
                }
                out = o.asm;
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
        return ExitCode::SUCCESS;
    }
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
        if matches!(args.mode, Mode::SyntaxOnly | Mode::PrintAst) {
            if pp.has_errors() {
                continue;
            }
            let (unit, diags) = loomcc_parse::parse(&toks);
            for d in &diags {
                eprintln!("{}", pp.sources.render(d));
            }
            failed |= diags.iter().any(|d| d.level == loomcc_pp::Level::Error);
            if args.mode == Mode::PrintAst {
                out.push_str(&loomcc_parse::print::print_unit(&unit));
                continue;
            }
            let name = input.file_name().map(|n| n.to_string_lossy().to_string()).unwrap_or_default();
            let (_hir, sdiags) = loomcc_sema::check(&unit, &name, loomcc_sema::types::Layout::snes());
            for d in &sdiags {
                eprintln!("{}", pp.sources.render(d));
            }
            failed |= sdiags.iter().any(|d| d.level == loomcc_pp::Level::Error);
            continue;
        }
        match args.mode {
            Mode::SyntaxOnly | Mode::PrintAst | Mode::EmitIr | Mode::Assembly | Mode::Interpret => unreachable!(),
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
