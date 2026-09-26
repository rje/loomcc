#!/usr/bin/env python3
"""Selects GCC gcc.c-torture/compile tests for loomcc (compile-only, T2/T3).

    external/fetch.sh gcc-torture && external/classify-gcc-compile.py

Same static filter as classify-gcc-torture.py (no floating point, 64-bit
integers, meaning-changing GNU extensions, library calls, dg directives),
then a dynamic one: clang --target=msp430-none-elf -std=c17
-pedantic-errors -Werror=vla -fsyntax-only (16-bit int) must accept the
file, so the test is valid C17 without VLAs at this target's widths. Wrappers (`loomcc-do: compile`, reference: tcc) go to
external/fetched/gcc-compile-wrapped/; names to external/gcc-torture-compile.list.
GPL-3.0-or-later: never copied into this repository.
"""
import importlib.util
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "external/fetched/gcc/gcc/testsuite/gcc.c-torture/compile"
WRAP = ROOT / "external/fetched/gcc-compile-wrapped"
spec = importlib.util.spec_from_file_location("cg", ROOT / "external/classify-gcc-torture.py")
cg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cg)


def clang16(p):
    r = subprocess.run(["taskpolicy", "-b", "nice", "-n", "19", "clang", "--target=msp430-none-elf", "-fsigned-char", "-std=c17", "-pedantic-errors", "-Werror=vla", "-fsyntax-only",
                        "-D__builtin_abort=abort", "-D__attribute__(x)=", str(p)], capture_output=True, text=True)
    return None if r.returncode == 0 else "clang (16-bit int) rejects it: " + (r.stderr.strip().splitlines() or ["?"])[0][-100:]


def main():
    tests = sorted(SRC.glob("*.c"))
    rejected, cands = {}, []
    for p in tests:
        why = cg.static_reason(p.read_text(errors="replace"))
        if why:
            rejected[p.name] = why
        else:
            cands.append(p)
    with ThreadPoolExecutor(max_workers=3) as ex:
        res = list(ex.map(clang16, cands))
    sel = []
    for p, why in zip(cands, res):
        if why:
            rejected[p.name] = why
        else:
            sel.append(p)
    if WRAP.exists():
        for old in WRAP.glob("*.c"):
            old.unlink()
    WRAP.mkdir(parents=True, exist_ok=True)
    for p in sel:
        (WRAP / p.name).write_text(
            "// loomcc-do: compile\n// loomcc-options: -D__builtin_abort=abort -D__builtin_exit=exit '-D__attribute__(x)='\n"
            "// loomcc-ref: tcc\n"
            f"// loomcc-source: GCC gcc/testsuite/gcc.c-torture/compile/{p.name} (GPL-3.0-or-later; fetched, not vendored)\n"
            f'#include "{p}"\n')
    (ROOT / "external/gcc-torture-compile.list").write_text(
        "# GCC gcc.c-torture/compile tests that apply at 16-bit int (external/classify-gcc-compile.py)\n" + "".join(p.name + "\n" for p in sel))
    (ROOT / "external/gcc-torture-compile.rejected").write_text(
        "# rejected GCC torture compile tests and why\n" + "".join(f"{n}\t{w}\n" for n, w in sorted(rejected.items())))
    print(f"{len(tests)} tests, {len(cands)} pass the static filter, {len(sel)} accepted by clang at 16-bit int")
    return 0


if __name__ == "__main__":
    sys.exit(main())
