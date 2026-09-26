#!/usr/bin/env python3
"""Wraps GCC's gcc.dg/cpp preprocessor tests as T1-style runner tests.

    external/fetch.sh gcc-torture && external/wrap-gcc-cpp.py

GPL-3.0-or-later: the tests are never copied; wrappers in
external/fetched/gcc-cpp-wrapped/ #include them. Selected: `dg-do
preprocess` tests with no dg-options, or only options that mean "standard C"
(-std=c99/c11/c17/gnu99/gnu11/gnu17, -pedantic, -pedantic-errors, -ansi is
excluded as C90). dg-error lines become `loomcc-error@<file>:<line>`
(`loomcc-diagnostic` under -pedantic-errors, where gcc's error is a pedantic
warning made fatal), dg-warning lines `loomcc-diagnostic`; diagnostics with
target selectors or relative line specs are not converted (the test is
dropped). Expected output: clang -E -P -std=c17 (tests where clang errors
without an expected error, or misses an expected diagnostic, are dropped).
Reference: clang only (816-tcc's preprocessor diverges too much, see
docs/DIVERGENCES.md).
"""
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "external/fetched/gcc/gcc/testsuite/gcc.dg/cpp"
OUT = ROOT / "external/fetched/gcc-cpp-wrapped"
OK_OPTS = {"-std=c99", "-std=c11", "-std=c17", "-std=c18", "-std=iso9899:1999", "-std=iso9899:2011",
           "-pedantic", "-pedantic-errors", "-Wall", ""}
# GNU-only behaviour (checked by hand): predefined __USER_LABEL_PREFIX__,
# push_macro/pop_macro and GCC pragmas, the `, ## __VA_ARGS__` extension,
# named variadic parameters (`args...`), #assert; UCN spelling in -E output
# (implementation-defined: clang prints UTF-8).
GNU_ONLY = re.compile(r"__USER_LABEL_PREFIX__|push_macro|pop_macro|GCC dependency|GCC system_header|,\s*##\s*__VA_ARGS__|\w\.\.\.\s*\)|#\s*(assert|unassert)|#\s*include\s*<|\\[uU][0-9a-fA-F]{4}")
DG = re.compile(r'dg-(error|warning|bogus|message)\s+"((?:[^"\\]|\\.)*)"(.*?)\}')


def main():
    if OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir(parents=True)
    selected, rejected = [], {}
    for src in sorted(SRC.glob("*.c")):
        text = src.read_text(errors="replace")
        if "dg-do preprocess" not in text:
            continue
        opts = re.findall(r'dg-options\s+"([^"]*)"', text) + re.findall(r'dg-options\s+(-\S+)', text)
        flat = " ".join(opts).split()
        if any(o not in OK_OPTS for o in flat) or re.search(r"dg-(additional-options|require|skip-if|final|xfail)", text):
            rejected[src.name] = "options, requirements or dg-final checks"
            continue
        if GNU_ONLY.search(text):
            rejected[src.name] = "GNU-only behaviour or a system header"
            continue
        pedantic_errors = "-pedantic-errors" in flat
        directives, bad = [], False
        for i, line in enumerate(text.split("\n"), 1):
            for m in DG.finditer(line):
                kind, rest = m.group(1), m.group(3)
                if kind in ("bogus", "message"):
                    continue
                if "target" in rest:
                    bad = True
                    continue
                num = re.search(r'""\s+\{[^}]*\}\s+(\d+)|""\s+(\d+)|\s(\d+)\s*$', rest.strip())
                at = i
                if num:
                    at = int(next(g for g in num.groups() if g))
                elif rest.strip():
                    bad = True
                if at == 0:
                    directives.append("// loomcc-diagnostic@*")
                    continue
                d = "error" if kind == "error" and not pedantic_errors else "diagnostic"
                directives.append(f"// loomcc-{d}@{src}:{at}")
        if bad:
            rejected[src.name] = "dg diagnostic form not converted"
            continue
        w = OUT / src.name
        w.write_text("// loomcc-do: preprocess\n// loomcc-ref: clang\n"
                     f"// loomcc-source: GCC gcc/testsuite/gcc.dg/cpp/{src.name} (GPL-3.0-or-later; fetched, not vendored)\n"
                     + "\n".join(directives) + ("\n" if directives else "") + f'#include "{src}"\n')
        r = subprocess.run(["nice", "-n", "19", "clang", "-E", "-P", "-std=c17", "-w", w.name], cwd=OUT, capture_output=True, text=True)
        if not directives:
            if r.returncode:
                rejected[src.name] = "clang rejects it"
                w.unlink()
                continue
            w.with_suffix(".expected").write_text(r.stdout)
        selected.append(w)
    # Keep only the wrappers whose clang reference agrees (diagnostic lines).
    js = OUT / "_refs.json"
    subprocess.run([str(ROOT / "run-tests"), "--refs-only", "-j", "3", "--json", str(js), str(OUT)], stdout=subprocess.DEVNULL)
    import json
    for line in js.read_text().splitlines():
        r = json.loads(line)
        if r["status"] != "PASS":
            name = r["test"].split("/")[-1]
            rejected[name] = "clang disagrees: " + r["detail"].splitlines()[0][:120]
            (OUT / name).unlink(missing_ok=True)
            (OUT / name).with_suffix(".expected").unlink(missing_ok=True)
    js.unlink()
    names = sorted(p.name for p in OUT.glob("*.c"))
    (ROOT / "external/gcc-dg-cpp.list").write_text("# gcc.dg/cpp tests wrapped by external/wrap-gcc-cpp.py\n" + "".join(n + "\n" for n in names))
    (ROOT / "external/gcc-dg-cpp.rejected").write_text("# gcc.dg/cpp preprocess tests not wrapped, and why\n"
                                                        + "".join(f"{n}\t{w}\n" for n, w in sorted(rejected.items())))
    print(f"{len(names)} gcc.dg/cpp tests wrapped; run with ./run-tests external/fetched/gcc-cpp-wrapped")
    return 0


if __name__ == "__main__":
    sys.exit(main())
