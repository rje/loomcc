#!/usr/bin/env python3
"""Runs every unit in testbed/loom/units.json through 816-tcc -E, 816-tcc -c
and clang -E with Loom's flags, in a scratch directory, and prints one line
per (sample, profile, unit). Exits nonzero if anything fails.

    nice -n 19 python3 testbed/scripts/check-units.py [scratch-dir] [--clang-only-report]
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

TESTBED = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))


def expand(arg, toolchain):
    for key, value in toolchain.items():
        arg = arg.replace("${" + key + "}", value)
    return arg


def run(argv, cwd, stdout_path=None):
    # 816-tcc -E aborts (SIGABRT) when given -o, so -E output goes to stdout.
    if stdout_path:
        with open(stdout_path, "w") as handle:
            result = subprocess.run(argv, cwd=cwd, stdout=handle, stderr=subprocess.PIPE, text=True)
        return result.returncode, result.stderr
    result = subprocess.run(argv, cwd=cwd, capture_output=True, text=True)
    return result.returncode, (result.stdout + result.stderr)


def merged_include(scratch, key, includes):
    """816-tcc 0.9.25 -E does not search every -I directory (observed: with the
    pvsneslib include dir behind other -I dirs, <snes.h> is "not found" under -E
    while -c finds it). -E therefore gets one directory holding the union of
    the roots, filled lowest priority first so the first root wins, as it does
    in the -I search order."""
    root = os.path.join(scratch, "include-" + key)
    if os.path.isdir(root):
        shutil.rmtree(root)
    os.makedirs(root)
    for include in reversed(includes):
        shutil.copytree(include[2:], root, dirs_exist_ok=True)
    return root


def main():
    scratch = sys.argv[1] if len(sys.argv) > 1 else tempfile.mkdtemp(prefix="loomcc-units-")
    os.makedirs(scratch, exist_ok=True)
    units = json.load(open(os.path.join(TESTBED, "loom/units.json")))
    toolchain = units["toolchain"]
    tcc = toolchain["compiler"]
    failures = 0
    summary = {}
    for name, sample in units["samples"].items():
        if sample["status"] != "generated":
            print(f"{name}: skipped ({sample['status']})")
            continue
        for profile, spec in sample["profiles"].items():
            includes = [expand(a, toolchain) for a in spec["argv_816tcc"] if a.startswith("-I")]
            includes = ["-I" + os.path.join(TESTBED, i[2:]) if not os.path.isabs(i[2:]) else i for i in includes]
            merged = merged_include(scratch, f"{name}-{profile}", includes)
            defines = ["-D" + d for d in spec["definitions"]]
            target = sample["target_flags"]
            counts = {"tcc_E": 0, "tcc_c": 0, "clang_E": 0, "total": 0}
            for unit in spec["c_units"]:
                source = os.path.join(TESTBED, unit["path"])
                stem = f"{name}-{profile}-" + unit["stable_name"].replace("/", "_").replace(".c", "")
                counts["total"] += 1
                results = {}
                code, out = run([tcc, "-I" + merged, *defines, *target, "-E", source], scratch, os.path.join(scratch, stem + ".tcc.i"))
                # 816-tcc -E aborts (exit 134) after writing complete output; count
                # it as a pass when nothing was reported and output was written.
                if code in (134, -6) and "error" not in out and os.path.getsize(os.path.join(scratch, stem + ".tcc.i")) > 0:
                    code = 0
                results["tcc_E"] = (code, out)
                code, out = run([tcc, *includes, *defines, *target, "-Wall", "-c", source, "-o", os.path.join(scratch, stem + ".ps")], scratch)
                results["tcc_c"] = (code, out)
                code, out = run(["clang", "-E", "-P", "-std=c99", *includes, *defines, source, "-o", os.path.join(scratch, stem + ".clang.i")], scratch)
                results["clang_E"] = (code, out)
                status = []
                for key, (code, out) in results.items():
                    if code == 0:
                        counts[key] += 1
                        status.append(f"{key}=ok")
                    else:
                        failures += 1
                        status.append(f"{key}=FAIL")
                line = f"{name} {profile} {unit['stable_name']}: " + " ".join(status)
                print(line)
                for key, (code, out) in results.items():
                    if out.strip():
                        tag = "error" if code else "warn"
                        for text in out.strip().splitlines()[:6]:
                            print(f"    [{key} {tag}] {text}")
            summary[f"{name}/{profile}"] = counts
    print(json.dumps(summary, indent=1))
    print("scratch:", scratch)
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
