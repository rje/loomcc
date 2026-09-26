//! Harness ROMs: assemble compiled units beside harness/rom/harness.asm and
//! PVSnesLib's crt0/libc, run the ROM in loom-emulator and read the outcome.

use crate::exec;
use crate::tools::Tools;
use std::path::{Path, PathBuf};
use std::time::Duration;

/// The macros every ROM build passes to the compiler.
pub const ROM_DEFINES: &[&str] =
    &["-Dmain=loomcc_test_main", "-Dabort=loomcc_test_abort", "-Dexit=loomcc_test_exit", "-DLOOMCC_TEST_ROM=1"];

const DONE_STATUS: i64 = 0x600d;
const ABORT_STATUS: i64 = 0xdead;
const EXIT_STATUS: i64 = 0xe817;
const CHECK_STATUS: i64 = 0xc4ec;
const OUTPUT_STATUS: i64 = 0x0bad;

/// harness/rom/stdio.c compiled once by 816-tcc (every ROM links it).
static STDIO_ASM: std::sync::Mutex<Option<String>> = std::sync::Mutex::new(None);

fn stdio_unit(tools: &Tools, dir: &Path, log: &mut String) -> Result<PathBuf, String> {
    let dst = dir.join("loomcc_stdio.asm");
    let mut cache = STDIO_ASM.lock().unwrap();
    if cache.is_none() {
        let tmp = dir.join("loomcc_stdio_build.asm");
        tcc_compile(tools, &tools.harness_rom.join("stdio.c"), &tmp, &[], dir, log)?;
        *cache = Some(std::fs::read_to_string(&tmp).map_err(|e| e.to_string())?);
    }
    std::fs::write(&dst, cache.as_ref().unwrap()).map_err(|e| e.to_string())?;
    Ok(dst)
}

/// The expected output as data for stdio.c's loomcc_check_output.
fn expected_unit(dir: &Path, expected: Option<&[u8]>) -> Result<PathBuf, String> {
    let bytes = expected.unwrap_or(&[]);
    let mut t = String::from(".include \"hdr.asm\"\n.SECTION \".loomcc_expected\" SUPERFREE\n");
    t.push_str(&format!("loomcc_expected_check: .db {}\n", if expected.is_some() { 1 } else { 0 }));
    t.push_str(&format!("loomcc_expected_len: .dw {}\n", bytes.len()));
    t.push_str("loomcc_expected_out:\n");
    for chunk in bytes.chunks(16) {
        t.push_str(".db ");
        t.push_str(&chunk.iter().map(|b| format!("${:02x}", b)).collect::<Vec<_>>().join(","));
        t.push('\n');
    }
    t.push_str(".db 0\n.ENDS\n");
    let dst = dir.join("loomcc_expected.asm");
    std::fs::write(&dst, t).map_err(|e| e.to_string())?;
    Ok(dst)
}

fn show(bytes: &[u8]) -> String {
    bytes.iter().map(|&b| if b == b'\n' { "\\n".to_string() } else if (32..127).contains(&b) { (b as char).to_string() } else { format!("\\x{:02x}", b) }).collect()
}

pub enum RomOutcome {
    Pass,
    Fail(String),
    /// The harness itself did not work (build or emulator trouble).
    Broken(String),
}

pub fn write_hdr(tools: &Tools, dir: &Path) -> Result<(), String> {
    let pvs = tools.pvsneslib.as_ref().ok_or("no PVSnesLib")?;
    let text = std::fs::read_to_string(pvs.join("devkitsnes/include/hdr.asm.in")).map_err(|e| e.to_string())?;
    let mut text = text;
    for (k, v) in [
        ("HIROMDEF", ""),
        ("FASTROMDEF", ".DEFINE FASTROM 1"),
        ("ROMTITLE", "LOOMCC TESTS"),
        ("CARTRIDGETYPE", "00"),
        ("ROMSIZE", "08"),
        ("SRAMSIZE", "00"),
        ("COUNTRY", "01"),
        ("LICENSEECODE", "00"),
        ("VERSION", "00"),
        ("ROMBANKS", "8"),
        ("ROMBANKSIZE", "8000"),
        ("ROMMODE", "LOROM"),
        ("ROMSPEED", "FASTROM"),
    ] {
        text = text.replace(&format!("@{}@", k), v);
    }
    std::fs::write(dir.join("hdr.asm"), text).map_err(|e| e.to_string())
}

/// 816-tcc + 816-opt: C to a WLA-DX unit.
pub fn tcc_compile(tools: &Tools, c: &Path, out_asm: &Path, flags: &[String], cwd: &Path, log: &mut String) -> Result<(), String> {
    let tcc = tools.tcc.as_ref().ok_or("no 816-tcc")?;
    let opt = tools.tcc_opt.as_ref().ok_or("no 816-opt")?;
    let ps = out_asm.with_extension("ps");
    let mut args: Vec<String> = flags.to_vec();
    args.extend(tools.tcc_includes());
    args.extend(["-F".into(), "-c".into(), c.display().to_string(), "-o".into(), ps.display().to_string()]);
    let o = exec::run(tcc, &args, cwd, Duration::from_secs(60));
    log_cmd(log, &o);
    if !o.ok() {
        return Err(format!("816-tcc failed ({}): {}", o.describe(), first_lines(&o.stderr, 5)));
    }
    let o = exec::run(opt, &["-i".into(), ps.display().to_string(), "-o".into(), out_asm.display().to_string()], cwd, Duration::from_secs(60));
    log_cmd(log, &o);
    if !o.ok() {
        return Err(format!("816-opt failed ({})", o.describe()));
    }
    Ok(())
}

pub fn log_cmd(log: &mut String, o: &exec::Output) {
    log.push_str("$ ");
    log.push_str(&o.cmdline);
    log.push('\n');
    if !o.stdout.trim().is_empty() {
        log.push_str(&first_lines(&o.stdout, 40));
        log.push('\n');
    }
    if !o.stderr.trim().is_empty() {
        log.push_str(&first_lines(&o.stderr, 40));
        log.push('\n');
    }
}

pub fn first_lines(s: &str, n: usize) -> String {
    // Drop tool chatter that is present on every run (816-opt banners,
    // wlalink's bank count and PVSnesLib's duplicate section labels).
    let v: Vec<&str> = s
        .lines()
        .filter(|l| !l.contains("816opt") && !l.contains("OBTAIN_ROMBANKS") && !l.contains("SECTIONSTART_.libc_mem") && !l.contains("SECTIONEND_.libc_mem"))
        .take(n)
        .collect();
    v.join("\n")
}

fn read_symbol(sym: &str, name: &str) -> Option<u32> {
    for line in sym.lines() {
        let mut it = line.split_whitespace();
        let (Some(a), Some(n)) = (it.next(), it.next()) else { continue };
        if n == name {
            return u32::from_str_radix(a, 16).ok();
        }
    }
    None
}

/// Assembles `units` (WLA-DX sources already in `dir`) with the harness,
/// links and runs the ROM.
pub fn link_and_run(tools: &Tools, dir: &Path, units: &[PathBuf], max_frames: u32, expected: Option<&[u8]>, log: &mut String) -> RomOutcome {
    macro_rules! tryb {
        ($e:expr) => {
            match $e {
                Ok(v) => v,
                Err(e) => return RomOutcome::Broken(e.to_string()),
            }
        };
    }
    tryb!(write_hdr(tools, dir));
    tryb!(std::fs::copy(tools.harness_rom.join("harness.asm"), dir.join("loomcc_harness.asm")));
    let wla = tools.wla.as_ref().unwrap();
    let mut objs = Vec::new();
    let mut all: Vec<PathBuf> = vec![dir.join("loomcc_harness.asm")];
    all.push(tryb!(stdio_unit(tools, dir, log)));
    all.push(tryb!(expected_unit(dir, expected)));
    all.extend(units.iter().cloned());
    for u in &all {
        let obj = u.with_extension("obj");
        let o = exec::run(
            wla,
            &["-d".into(), "-s".into(), "-x".into(), "-o".into(), obj.display().to_string(), u.display().to_string()],
            dir,
            Duration::from_secs(60),
        );
        log_cmd(log, &o);
        if !o.ok() {
            // An assembler error in a compiled unit is the compiler's fault.
            let msg = format!("wla-65816 rejected {}: {}", u.file_name().unwrap().to_string_lossy(), first_lines(&o.stderr, 5));
            let ours = u.file_name().map_or(false, |n| n.to_string_lossy().starts_with("loomcc_"));
            return if ours { RomOutcome::Broken(msg) } else { RomOutcome::Fail(msg) };
        }
        objs.push(obj);
    }
    let pvs = tools.pvsneslib.as_ref().unwrap();
    let lib = pvs.join("pvsneslib/lib/LoROM_FastROM");
    let mut linkfile = String::from("[objects]\n");
    for o in &objs {
        linkfile.push_str(&format!("{}\n", o.display()));
    }
    for l in ["crt0_snes.obj", "libc.obj", "libm.obj", "libtcc.obj"] {
        linkfile.push_str(&format!("{}\n", lib.join(l).display()));
    }
    tryb!(std::fs::write(dir.join("linkfile"), linkfile));
    let o = exec::run(
        tools.wlalink.as_ref().unwrap(),
        &["-d".into(), "-s".into(), "-A".into(), "-c".into(), "-L".into(), lib.display().to_string(), "linkfile".into(), "test.sfc".into()],
        dir,
        Duration::from_secs(60),
    );
    log_cmd(log, &o);
    if !o.ok() || !dir.join("test.sfc").exists() {
        return RomOutcome::Fail(format!("wlalink failed: {}", first_lines(&o.stderr, 5)));
    }
    let sym = tryb!(std::fs::read_to_string(dir.join("test.sym")));
    let (Some(done), Some(status), Some(result), Some(outlen), Some(diff)) = (
        read_symbol(&sym, "test_done"),
        read_symbol(&sym, "test_status"),
        read_symbol(&sym, "test_result"),
        read_symbol(&sym, "loomcc_out_len"),
        read_symbol(&sym, "loomcc_diff"),
    ) else {
        return RomOutcome::Broken("harness symbols missing from test.sym".into());
    };
    tryb!(std::fs::write(dir.join("script.json"), format!("[{{\"until\":\"done=1\",\"max\":{}}}]", max_frames)));
    let emu_args: Vec<String> = vec![
        "trace".into(),
        "--rom".into(),
        "test.sfc".into(),
        "--script".into(),
        "script.json".into(),
        "--out".into(),
        "emu".into(),
        "--watches".into(),
        format!(
            "done:{:06x}:2,status:{:06x}:2,result:{:06x}:s2,outlen:{:06x}:2,d0:{:06x}:4,d1:{:06x}:4,d2:{:06x}:4,d3:{:06x}:4",
            done, status, result, outlen, diff, diff + 4, diff + 8, diff + 12
        ),
    ];
    // loom-emulator occasionally faults at frame 0 when the machine is busy
    // ("MesenCore frame step advanced from 0 to 0"): retry once.
    let mut o = exec::run(tools.emulator.as_ref().unwrap(), &emu_args, dir, Duration::from_secs(120));
    if !o.ok() && o.stderr.contains("frame 0") {
        log_cmd(log, &o);
        o = exec::run(tools.emulator.as_ref().unwrap(), &emu_args, dir, Duration::from_secs(120));
    }
    log_cmd(log, &o);
    if !o.ok() {
        return RomOutcome::Broken(format!("loom-emulator failed ({}): {}", o.describe(), first_lines(&o.stderr, 5)));
    }
    let csv = tryb!(std::fs::read_to_string(dir.join("emu/trace.csv")));
    let last = csv.lines().last().unwrap_or("");
    let cols: Vec<i64> = last.split(',').skip(2).filter_map(|v| v.parse().ok()).collect();
    if cols.len() != 8 {
        return RomOutcome::Broken(format!("unreadable trace row `{}`", last));
    }
    let (d, s, r, outlen) = (cols[0], cols[1], cols[2], cols[3]);
    let mut window = Vec::new();
    for w in &cols[4..8] {
        window.extend_from_slice(&(*w as u32).to_le_bytes());
    }
    let frames = csv.lines().count().saturating_sub(1);
    if d != 1 {
        return RomOutcome::Fail(format!("did not finish within {} frames (status {:04x})", frames, s));
    }
    match s {
        DONE_STATUS if r == 0 => RomOutcome::Pass,
        DONE_STATUS => RomOutcome::Fail(format!("main returned {}", r)),
        ABORT_STATUS => RomOutcome::Fail("abort() called".into()),
        EXIT_STATUS if r == 0 => RomOutcome::Pass,
        EXIT_STATUS => RomOutcome::Fail(format!("exit({})", r)),
        CHECK_STATUS => RomOutcome::Fail(format!("CHECK failed at line {}", r)),
        OUTPUT_STATUS => {
            let at = (r as u16) as usize;
            let exp = expected.unwrap_or(&[]);
            let e = &exp[at.min(exp.len())..(at + 16).min(exp.len())];
            let got_len = (outlen as usize).saturating_sub(at).min(16);
            RomOutcome::Fail(format!(
                "output differs at byte {} (printed {} bytes, expected {}): got `{}`, expected `{}`",
                at,
                outlen,
                exp.len(),
                show(&window[..got_len]),
                show(e)
            ))
        }
        other => RomOutcome::Broken(format!("unknown status {:04x}", other)),
    }
}
