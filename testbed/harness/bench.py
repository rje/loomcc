#!/usr/bin/env python3
"""loomcc testbed: build one benchmark ROM in one variant, run it in
loom-emulator, and report code bytes, instructions and master clocks.

    bench.py <bench-dir> <variant> <outdir>     variant: tcc | asm | loomcc | host

run.sh is the entry point; this file holds the logic. See
testbed/bench/README.md for the benchmark contract.
"""

import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tomllib
from pathlib import Path

HARNESS = Path(__file__).resolve().parent
TESTBED = HARNESS.parent
REPO = TESTBED.parent

PVS = Path(os.environ.get(
    "PVSNESLIB_HOME", Path.home() / "Library/Loom/Toolchains/v0/artifacts/pvsneslib"))
TCC = PVS / "devkitsnes/bin/816-tcc"
WLA = PVS / "devkitsnes/bin/wla-65816"
WLALINK = PVS / "devkitsnes/bin/wlalink"
OPT = PVS / "devkitsnes/tools/816-opt"
LIBDIR = PVS / "pvsneslib/lib/LoROM_FastROM"
LIBS = ["crt0_snes.obj", "libc.obj", "libm.obj", "libtcc.obj"]
SYS_INCLUDES = [PVS / "pvsneslib/include", PVS / "devkitsnes/include"]
# loom-emulator, from a Loom checkout ($LOOM_REPO) built with --features mesen-core.
EMULATOR = Path(os.environ.get(
    "LOOM_EMULATOR",
    Path(os.environ.get("LOOM_REPO", REPO.parent / "loom")) / "target/debug/loom-emulator"))
# The loomcc variant compiles unit.c with this command:
#   $LOOMCC -S [-I dir]... [-D def]... unit.c -o unit.asm
LOOMCC = os.environ.get("LOOMCC", str(REPO / "target/debug/loomcc"))

NICE = ["taskpolicy", "-b", "nice", "-n", "19"]
# The emulator runs under plain nice -n 10: in the background QoS band
# (taskpolicy -b) it gets too little CPU to produce a frame within
# MesenCore's five-second limit. Compilers and assemblers stay in the band.
EMU_NICE = ["nice", "-n", "10"]
DONE = 0x600D
MAX_FRAMES = 1200
SCANLINE_CLOCKS = 1364
DOT_CLOCKS = 4
LINES = 262
UNIT_PREFIX = "UNIT_"
HARNESS_LABELS = {
    "main", "harness_clear_out", "harness_after_run", "harness_idle",
    "harness_wait_vblank", "harness_wait_leave", "harness_wait_enter",
    "?", "(unlabelled)",
}


class BenchError(Exception):
    pass


def run(cmd, cwd=None, capture=True):
    result = subprocess.run([str(c) for c in cmd], cwd=cwd, capture_output=capture, text=True)
    if result.returncode != 0:
        raise BenchError(
            f"command failed ({result.returncode}): {' '.join(str(c) for c in cmd)}\n"
            f"{result.stdout}\n{result.stderr}")
    return result


def load_config(bench):
    for name in ("bench.toml", "bench.json"):
        path = bench / name
        if path.exists():
            if name.endswith(".toml"):
                return tomllib.loads(path.read_text())
            return json.loads(path.read_text())
    raise BenchError(f"{bench}: no bench.toml or bench.json")


def include_flags(bench, config):
    dirs = [HARNESS, bench] + [(bench / d).resolve() for d in config.get("include_dirs", [])]
    return [f"-I{d}" for d in dirs]


def define_flags(config, variant):
    defs = list(config.get("defines", []))
    if variant == "host":
        defs += config.get("host_defines", [])
    else:
        defs += config.get("snes_defines", [])
    return [f"-D{d}" for d in defs]


def write_hdr(build):
    text = (PVS / "devkitsnes/include/hdr.asm.in").read_text()
    for key, value in {
        "HIROMDEF": "", "FASTROMDEF": ".DEFINE FASTROM 1", "ROMTITLE": "LOOMCC BENCH",
        "CARTRIDGETYPE": "00", "ROMSIZE": "08", "SRAMSIZE": "00", "COUNTRY": "01",
        "LICENSEECODE": "00", "VERSION": "00", "ROMBANKS": "8", "ROMBANKSIZE": "8000",
        "ROMMODE": "LOROM", "ROMSPEED": "FASTROM",
    }.items():
        text = text.replace(f"@{key}@", value)
    (build / "hdr.asm").write_text(text)


def tcc_to_asm(c_file, asm_out, flags, build):
    ps = asm_out.with_suffix(".ps")
    run(NICE + [TCC] + flags + [f"-I{d}" for d in SYS_INCLUDES]
        + ["-F", "-Wall", "-c", c_file, "-o", ps], cwd=build)
    run(NICE + [OPT, "-i", ps, "-o", asm_out], cwd=build)


SECTION_RE = re.compile(r'^(\s*\.section\s+")([^"]+)(")', re.IGNORECASE)


def mark_unit_sections(asm_in, asm_out):
    """Prefix every ROM section the unit defines with UNIT_ so the linked
    symbol file bounds exactly the unit's code (SECTIONSTART_/SECTIONEND_).
    Sections that APPENDTO another (initialised data) keep their names."""
    lines = []
    for line in Path(asm_in).read_text().splitlines():
        match = SECTION_RE.match(line)
        if match and "appendto" not in line.lower():
            line = f"{match.group(1)}{UNIT_PREFIX}{match.group(2)}{match.group(3)}" + line[match.end():]
        lines.append(line)
    Path(asm_out).write_text("\n".join(lines) + "\n")


def build_rom(bench, variant, config, build):
    if build.exists():
        shutil.rmtree(build)
    build.mkdir(parents=True)
    write_hdr(build)
    flags = include_flags(bench, config) + define_flags(config, variant)

    shutil.copy(HARNESS / "harness.asm", build / "harness.asm")
    tcc_to_asm(bench / "driver.c", build / "driver.asm", flags, build)

    raw = build / "unit.raw.asm"
    if variant == "tcc":
        tcc_to_asm(bench / "unit.c", raw, flags, build)
    elif variant == "asm":
        if not (bench / "unit.asm").exists():
            raise BenchError(f"{bench}: no unit.asm for the asm variant")
        shutil.copy(bench / "unit.asm", raw)
    elif variant == "loomcc":
        # >>> loomcc plugs in here: any compiler that turns unit.c into a
        # WLA-DX unit (starting `.include "hdr.asm"`, 816-tcc ABI entries).
        run(NICE + [LOOMCC, "-S"] + flags + [bench / "unit.c", "-o", raw], cwd=build)
    else:
        raise BenchError(f"unknown variant {variant}")
    mark_unit_sections(raw, build / "unit.asm")

    for name in ("harness", "driver", "unit"):
        run(NICE + [WLA, "-d", "-s", "-x", "-o", f"{name}.obj", f"{name}.asm"], cwd=build)
    (build / "linkfile").write_text(
        "[objects]\nharness.obj\ndriver.obj\nunit.obj\n"
        + "".join(f"{LIBDIR / lib}\n" for lib in LIBS))
    link = run(NICE + [WLALINK, "-d", "-s", "-A", "-c", "-L", LIBDIR, "linkfile", "bench.sfc"], cwd=build)
    (build / "link.log").write_text(link.stdout + link.stderr)
    return build / "bench.sfc", build / "bench.sym"


def read_symbols(sym_path):
    symbols = []
    for line in Path(sym_path).read_text().splitlines():
        parts = line.split()
        if len(parts) < 2 or line.startswith(";"):
            continue
        try:
            address = int(parts[0], 16)
        except ValueError:
            continue
        symbols.append((address, parts[1]))
    return symbols


def is_rom(address):
    return (address & 0xFFFF) >= 0x8000 and (address >> 16) not in (0x7E, 0x7F)


def section_ranges(symbols, prefix):
    starts, ends = {}, {}
    for address, name in symbols:
        if name.startswith("SECTIONSTART_" + prefix):
            starts[name[len("SECTIONSTART_"):]] = address
        elif name.startswith("SECTIONEND_" + prefix):
            ends[name[len("SECTIONEND_"):]] = address
    return {name: (starts[name], ends[name]) for name in starts if name in ends}


def enclosing_section(symbols, address):
    """The (start, end) of the section holding a code address."""
    best = None
    ranges = {}
    for sym_address, name in symbols:
        if name.startswith("SECTIONSTART_"):
            ranges.setdefault(name[13:], [None, None])[0] = sym_address
        elif name.startswith("SECTIONEND_"):
            ranges.setdefault(name[11:], [None, None])[1] = sym_address
    for start, end in ranges.values():
        if start is not None and end is not None and start <= address < end:
            if best is None or end - start < best[1] - best[0]:
                best = (start, end)
    return best


def symbol_address(symbols, name):
    found = [a for a, n in symbols if n == name]
    if not found:
        raise BenchError(f"symbol {name} not in the symbol file")
    return found[0]


def emulate(rom, sym, out, watches, profile=None):
    out.mkdir(parents=True, exist_ok=True)
    script = out / "script.json"
    script.write_text(json.dumps([{"until": f"done={DONE}", "max": MAX_FRAMES}]))
    cmd = EMU_NICE + [EMULATOR, "trace", "--rom", rom, "--script", script, "--out", out,
                  "--watches", ",".join(f"{n}:{a:06x}:{w}" for n, a, w in watches)]
    if profile:
        cmd += ["--profile", sym, "--profile-from", profile[0], "--profile-to", profile[1]]
    # MesenCore gives up on a frame after five seconds of wall time, which a
    # background-priority run can exceed; and the emulator has occasionally
    # printed its usage and exited on a valid command line under load. Retry
    # both (a result is only ever taken from a run that finished).
    transient = ("NoFrame", "usage", "debug-probe")
    for attempt in range(4):
        try:
            result = run(cmd)
            break
        except BenchError as error:
            if not any(t in str(error) for t in transient) or attempt == 3:
                raise
    (out / "emulator.log").write_text(result.stdout + result.stderr)
    rows = (out / "trace.csv").read_text().splitlines()
    header = rows[0].split(",")
    return [dict(zip(header, (int(v, 16) if k == "pad" else int(v) for k, v in zip(header, row.split(",")))))
            for row in rows[1:]]


def read_profile(path):
    text = Path(path).read_text().splitlines()
    match = re.match(r"(\d+) frames, (\d+) instructions a frame", text[0])
    frames, per_frame = int(match.group(1)), int(match.group(2))
    counts = {}
    for line in text[2:]:
        if not line.strip():
            break
        parts = line.split(None, 2)
        counts[parts[2]] = round(float(parts[0]) * frames)
    return frames, per_frame, counts


def measure(bench, variant, outdir):
    config = load_config(bench)
    outdir.mkdir(parents=True, exist_ok=True)
    rom, sym = build_rom(bench, variant, config, outdir / "build")
    symbols = read_symbols(sym)

    watch = [(n, symbol_address(symbols, "bench_" + n), 2)
             for n in ("phase", "done", "h0", "v0", "h1", "v1", "wrap")]
    watch.append(("count", symbol_address(symbols, "bench_out_count"), 2))
    out_base = symbol_address(symbols, "bench_out")
    watch += [(f"out{i}", out_base + 2 * i, 2) for i in range(32)]

    first = emulate(rom, sym, outdir / "run", watch)
    last = first[-1]
    if last["done"] != DONE:
        raise BenchError(f"the ROM never finished (bench_phase {last['phase']} after {len(first)} frames)")
    phases = [row["phase"] for row in first]
    start = phases.index(next(p for p in phases if p >= 2))
    run_from = max(i for i in range(start) if phases[i] <= 1) + 1 if start else 0
    run_to = next(i for i, p in enumerate(phases) if p >= 3)

    # The profiler charges each instruction to the nearest label below it and
    # ignores labels starting "__", "_far", "_skip" or holding "@". A label
    # "sec:<name>" at every ROM section start makes sure no section's code is
    # charged to a label from another section; classification below is by
    # the label's address.
    profile_sym = sym.with_name("profile.sym")
    extra = [f"{a:08x} sec:{n[len('SECTIONSTART_'):]}" for a, n in symbols
             if n.startswith("SECTIONSTART_") and is_rom(a)]
    profile_sym.write_text(sym.read_text() + "\n".join(extra) + "\n")
    symbols = read_symbols(profile_sym)
    second = emulate(rom, profile_sym, outdir / "profile", watch, profile=(run_from, run_to + 1))
    if second[-1] != last:
        raise BenchError("the profiled run differs from the first run")
    frames, per_frame, counts = read_profile(outdir / "profile" / "profile.txt")
    if per_frame >= 29000:
        raise BenchError(f"{per_frame} instructions a frame: near the emulator's 30000 trace cap")

    # The unit: every label inside its UNIT_ sections, plus the helpers it calls.
    unit_sections = {n: r for n, r in section_ranges(symbols, UNIT_PREFIX).items() if is_rom(r[0])}
    unit_ranges = list(unit_sections.values())
    helper_ranges = []
    names = {n for _, n in symbols}
    for helper in config.get("helper_labels", []):
        if helper not in names:
            continue  # not linked in this variant (nothing calls it)
        address = symbol_address(symbols, helper)
        section = enclosing_section(symbols, address)
        helper_ranges.append(section if section else (address, address + 1))

    def in_ranges(address, ranges):
        return any(s <= address < e for s, e in ranges)

    unit_labels, helper_labels = set(), set()
    for address, name in symbols:
        if not is_rom(address) or name.startswith("SECTION"):
            continue
        if in_ranges(address, unit_ranges):
            unit_labels.add(name)
        elif in_ranges(address, helper_ranges):
            helper_labels.add(name)
    for name in config.get("unit_labels", []):
        if name not in unit_labels:
            raise BenchError(f"unit label {name} is not defined by the unit")

    unit_instr = sum(c for n, c in counts.items() if n in unit_labels)
    helper_instr = sum(c for n, c in counts.items() if n in helper_labels)
    non_harness = {n: c for n, c in counts.items() if n not in HARNESS_LABELS}

    v0, v1, h0, h1 = last["v0"], last["v1"], last["h0"], last["h1"]
    dv = (v1 - v0) % LINES
    elapsed = dv * SCANLINE_CLOCKS + (h1 - h0) * DOT_CLOCKS
    if last["wrap"] or elapsed <= 0 or run_to - run_from > 2:
        raise BenchError(
            f"bench_run took about a frame or longer (line 224 came round during it; frames "
            f"{run_from}..{run_to}): it must fit in one frame (357368 master clocks)")

    code = {n[len(UNIT_PREFIX):]: e - s for n, (s, e) in sorted(unit_sections.items())}
    rodata = sum(b for n, b in code.items() if "rodata" in n)
    out_words = [last[f"out{i}"] for i in range(min(last["count"], 32))]
    return {
        "bench": config.get("name", bench.name),
        "variant": variant,
        "out": out_words,
        "code_bytes": sum(code.values()) - rodata,
        "rodata_bytes": rodata,
        "sections": code,
        "unit_instructions": unit_instr,
        "helper_instructions": helper_instr,
        "unit_and_helper_instructions": unit_instr + helper_instr,
        "non_harness_instructions": sum(non_harness.values()),
        "labels": dict(sorted(non_harness.items(), key=lambda kv: -kv[1])),
        "latch": {"h0": h0, "v0": v0, "h1": h1, "v1": v1},
        "elapsed_master_clocks": elapsed,
        "profile_frames": [run_from, run_to],
        "rom": str(rom),
    }


def calibration():
    """The harness's fixed clocks (empty bench_run), cached per harness."""
    key = hashlib.sha256((HARNESS / "harness.asm").read_bytes()
                         + (HARNESS / "calib" / "driver.c").read_bytes()
                         + str(PVS).encode() + str(EMULATOR).encode()).hexdigest()[:16]
    cache = Path(os.environ.get("LOOMCC_BENCH_CACHE",
                                Path(os.environ.get("TMPDIR", "/tmp")) / "loomcc-bench"))
    path = cache / f"calib-{key}.json"
    if path.exists():
        return json.loads(path.read_text())
    result = measure(HARNESS / "calib", "tcc", cache / f"calib-{key}")
    value = {"elapsed_master_clocks": result["elapsed_master_clocks"],
             "non_harness_instructions": result["non_harness_instructions"]}
    path.write_text(json.dumps(value))
    return value


def host(bench, outdir):
    config = load_config(bench)
    outdir.mkdir(parents=True, exist_ok=True)
    exe = outdir / "host"
    run(NICE + ["clang", "-std=gnu99", "-O1", "-w"] + include_flags(bench, config)
        + define_flags(config, "host")
        + [HARNESS / "host_main.c", bench / "unit.c", bench / "driver.c", "-o", exe])
    words = [int(w) for w in run([exe]).stdout.split()]
    return {"bench": config.get("name", bench.name), "variant": "host", "out": words}


def main(argv):
    if len(argv) != 4:
        print(__doc__, file=sys.stderr)
        return 2
    bench, variant, outdir = Path(argv[1]).resolve(), argv[2], Path(argv[3]).resolve()
    try:
        if variant == "host":
            result = host(bench, outdir)
        else:
            result = measure(bench, variant, outdir)
            calib = calibration()
            result["harness_overhead_master_clocks"] = calib["elapsed_master_clocks"]
            result["net_master_clocks"] = result["elapsed_master_clocks"] - calib["elapsed_master_clocks"]
    except BenchError as error:
        print(f"bench: {bench.name} [{variant}]: {error}", file=sys.stderr)
        return 1
    (outdir / "results.json").write_text(json.dumps(result, indent=2) + "\n")
    summary = {k: result[k] for k in ("bench", "variant", "code_bytes", "rodata_bytes",
                                       "unit_instructions", "helper_instructions",
                                       "non_harness_instructions", "elapsed_master_clocks",
                                       "net_master_clocks") if k in result}
    print(json.dumps(summary))
    print("out:", " ".join(f"{w:04x}" for w in result["out"]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
