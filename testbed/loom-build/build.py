#!/usr/bin/env python3
"""Rebuild a Loom project's release ROM outside the Loom repo, with 816-tcc
or loomcc compiling the C units.

    build.py --project <copy of examples/X, packaged by loom-automation>
             --runtime <snapshot of loom/runtime>
             --variant lst|tcc|loomcc [--tcc-units a.c,b.c] --out <dir>

The unit list and order come from the automation build's
Build/Release/project/<id>/loom-project.build.json, the exact pipeline from
loom's crates/loom-toolchain/src/pvs_project.rs (compile_cached_unit,
link_project):

  816-tcc -I<roots> -DNDEBUG=1 -DLOOM_TARGET_PVSNESLIB=1
          -DLOOM_GENERATED_POOLS=1 -F -Wall -c loom_<stem>.c -o loom_<stem>.ps
  816-opt -i .ps -o .asm; section names .rodata/.rel.rodata/.bss get the
  unit stem; wla-65816 -d -s -x -I<stage> -I<roots> -I<project> -o unit.obj
  wlalink -d -s -A -c -L <lib> linkfile out.sfc  (objects in unit order,
  then crt0_snes libc libm libtcc).

Variants:
  lst     assemble each unit's assembly as recorded in loom-project.lst
          (must reproduce the automation ROM byte for byte: checks this script)
  tcc     compile the C units from --runtime/--project with 816-tcc
  loomcc  compile the C units with loomcc as ONE whole program (inlining
          across units); units named in --tcc-units stay with 816-tcc.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

PVS = Path(os.environ.get("PVSNESLIB_HOME", Path.home() / "Library/Loom/Toolchains/v0/artifacts/pvsneslib"))
TCC = PVS / "devkitsnes/bin/816-tcc"
OPT = PVS / "devkitsnes/tools/816-opt"
WLA = PVS / "devkitsnes/bin/wla-65816"
LINK = PVS / "devkitsnes/bin/wlalink"
LIB = PVS / "pvsneslib/lib/LoROM_FastROM"
LOOMCC = Path(os.environ.get("LOOMCC", Path(__file__).resolve().parents[2] / "target/debug/loomcc"))
BG = ["taskpolicy", "-b", "nice", "-n", "19"]
DEFS = ["-DNDEBUG=1", "-DLOOM_TARGET_PVSNESLIB=1", "-DLOOM_GENERATED_POOLS=1"]


def run(cmd, cwd, check=True):
    p = subprocess.run(BG + [str(c) for c in cmd], cwd=cwd, capture_output=True, text=True)
    if check and p.returncode != 0:
        sys.exit(f"failed: {' '.join(map(str, cmd))}\n{p.stdout}\n{p.stderr}")
    return p


def stem(name):
    return "loom_" + hashlib.sha256(name.encode()).hexdigest()[:16]


def namespace(text, st):
    return (text.replace('.SECTION ".rodata" SUPERFREE', f'.SECTION ".rodata.{st}" SUPERFREE')
                .replace('.SECTION ".rel.rodata" SUPERFREE', f'.SECTION ".rel.rodata.{st}" SUPERFREE')
                .replace('.RAMSECTION ".bss" BANK $7e SLOT 2', f'.RAMSECTION ".bss.{st}" BANK $7e SLOT 2'))


def lst_units(lst):
    units, cur, name = {}, None, None
    for line in lst.read_text(errors="replace").splitlines():
        if line.startswith("; unit "):
            name = line[7:].strip()
            cur = units[name] = []
            continue
        if cur is not None:
            cur.append(line[8:] if re.match(r"^\d{6}  ", line) else line)
    return {k: "\n".join(v) + "\n" for k, v in units.items()}


def source_path(unit, project, runtime):
    if unit.startswith(".loom/"):
        return project / unit
    if unit.startswith("project/"):
        return project / unit[len("project/"):]
    if unit.startswith("runtime/"):
        return runtime / unit[len("runtime/"):]
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project", required=True, type=Path)
    ap.add_argument("--runtime", required=True, type=Path)
    ap.add_argument("--variant", required=True, choices=["lst", "tcc", "loomcc"])
    ap.add_argument("--tcc-units", default="")
    ap.add_argument("--loomcc-flags", default="")
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--profile", default="release", choices=["release", "debug"])
    ap.add_argument("--reference-build", type=Path, help="take the unit list and build identity from this project copy's packaged build")
    a = ap.parse_args()
    global DEFS
    if a.profile == "debug":
        DEFS = ["-DLOOM_BUILD_DEBUG=1", "-DLOOM_TARGET_PVSNESLIB=1", "-DLOOM_GENERATED_POOLS=1"]
    project, runtime, out = a.project.resolve(), a.runtime.resolve(), a.out.resolve()
    rel = (a.reference_build or project) / "Build/Release/project"
    build_dir = next(rel.iterdir()) if rel.exists() and any(rel.iterdir()) else None
    if build_dir is not None:
        build = json.loads((build_dir / "loom-project.build.json").read_text())
        units = list(build["cache"]["compiled_units"])
    else:
        # No packaged ROM (loom-automation's release step can time out at
        # background priority after generating the sources): derive the
        # unit list the way pvs_project.rs does, sorted by stable name, with
        # the build identity last.
        inputs = json.loads((project / ".loom/generated/toolchain/pvsneslib-inputs.json").read_text())["inputs"]
        names = set(inputs["generated_c_sources"] + inputs["generated_assembly_sources"]
                    + inputs["loom_install_portable_sources"] + inputs["loom_install_target_sources"])
        for root in inputs["authored_source_roots"]:
            for f in sorted((project / root).rglob("*")):
                if f.suffix in (".c", ".asm"):
                    names.add("project/" + str(f.relative_to(project)))
        units = sorted(names) + ["generated/build-identity.asm"]
    if a.profile == "debug":
        # The debug profile adds the replay passthrough (Loom's
        # loom_install_debug_sources) in sorted position.
        units.insert(units.index("runtime/src/scene.c"), "runtime/src/replay-passthrough.c")
    if build_dir is not None:
        lst = lst_units(build_dir / "loom-project.lst")
    else:
        lst = {"generated/build-identity.asm": '.include "hdr.asm"\n\n.SECTION "loom.build.identity" SUPERFREE KEEP\nloom_build_identity_start:\n  .db "loomcc-testbed"\nloom_build_identity_end:\n  .db 0\n.ENDS\n'}
    gen = project / ".loom/generated"
    roots = [runtime / "include", runtime / "backends/pvsneslib/include", gen / "include",
             PVS / "pvsneslib/include", PVS / "devkitsnes/include"]
    if out.exists():
        shutil.rmtree(out)
    stage = out / "stage"
    stage.mkdir(parents=True)
    shutil.copy(gen / "target/pvsneslib/hdr.asm", stage / "hdr.asm")
    tcc_units = set(u for u in a.tcc_units.split(",") if u)
    c_units = [u for u in units if u.endswith(".c")]
    asm_text = {}
    record = {}
    if a.variant == "loomcc":
        whole = [u for u in c_units if u not in tcc_units]
        srcs = [source_path(u, project, runtime) for u in whole]
        flags = [f"-I{r}" for r in roots] + DEFS + a.loomcc_flags.split()
        scans = [source_path(u, project, runtime) for u in units if u.endswith(".asm") and u.startswith("runtime/")]
        cmd = [LOOMCC, "-S"] + flags + [f"--asm-callbacks={s}" for s in scans] + srcs + ["-o", stage / "loomcc_whole.asm"]
        p = run(cmd, stage, check=False)
        (out / "loomcc.log").write_text(p.stdout + p.stderr)
        if p.returncode != 0:
            sys.exit(f"loomcc failed:\n{p.stderr[-4000:]}")
        for u in whole:
            record[u] = "loomcc"
    for u in units:
        st = stem(u)
        if a.variant == "lst" or u == "generated/build-identity.asm":
            asm_text[u] = lst[u]
            record.setdefault(u, "lst")
            continue
        if u.endswith(".asm"):
            asm_text[u] = source_path(u, project, runtime).read_text()
            record[u] = "hand asm"
            continue
        if a.variant == "loomcc" and u not in tcc_units:
            continue
        src = source_path(u, project, runtime)
        shutil.copy(src, stage / f"{st}.c")
        run([TCC] + [f"-I{r}" for r in roots] + DEFS + ["-F", "-Wall", "-c", f"{st}.c", "-o", f"{st}.ps"], stage)
        run([OPT, "-i", f"{st}.ps", "-o", f"{st}.asm"], stage)
        asm_text[u] = namespace((stage / f"{st}.asm").read_text(), st)
        record[u] = "816-tcc"
    objs = []
    for i, u in enumerate(units):
        if u not in asm_text:
            continue
        st = stem(u)
        (stage / f"{st}.asm").write_text(asm_text[u])
        obj = f"object-{i:03}.obj"
        run([WLA, "-d", "-s", "-x", "-I", stage] + sum([["-I", r] for r in roots], []) + ["-I", project, "-o", obj, f"{st}.asm"], stage)
        objs.append(obj)
    if a.variant == "loomcc":
        run([WLA, "-d", "-s", "-x", "-I", stage] + sum([["-I", r] for r in roots], []) + ["-I", project, "-o", "loomcc.obj", "loomcc_whole.asm"], stage)
        objs.append("loomcc.obj")
    link = "[objects]\n" + "".join(o + "\n" for o in objs) + "".join(str(LIB / l) + "\n" for l in ["crt0_snes.obj", "libc.obj", "libm.obj", "libtcc.obj"])
    (stage / "linkfile").write_text(link)
    p = run([LINK, "-d", "-s", "-A", "-c", "-L", LIB, "linkfile", "loom-project.sfc"], stage)
    (out / "link.log").write_text(p.stdout + p.stderr)
    for f in ["loom-project.sfc", "loom-project.sym"]:
        shutil.copy(stage / f, out / f)
    (out / "units.json").write_text(json.dumps(record, indent=1) + "\n")
    rom = (out / "loom-project.sfc").read_bytes()
    if build_dir is None:
        print(f"{a.variant}: {len(rom)} bytes, sha256 {hashlib.sha256(rom).hexdigest()[:16]} (no packaged ROM to compare)")
        return
    ref = (build_dir / "loom-project.sfc").read_bytes()
    if a.profile == "debug":
        print(f"{a.variant} debug: {len(rom)} bytes, sha256 {hashlib.sha256(rom).hexdigest()[:16]}")
        return
    print(f"{a.variant}: {len(rom)} bytes, sha256 {hashlib.sha256(rom).hexdigest()[:16]}, "
          f"{'IDENTICAL to' if rom == ref else 'differs from'} the automation ROM")


if __name__ == "__main__":
    main()
