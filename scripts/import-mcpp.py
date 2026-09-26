#!/usr/bin/env python3
"""Imports mcpp's cpp-test suite (BSD-2-Clause) into tests/t1-pp/mcpp.

    external/fetch.sh mcpp && scripts/import-mcpp.py

mcpp 2.7.2's cpp-test/test-t holds GCC-dg-style preprocessor tests:
n_* (conforming behaviour), e_* (errors that must be diagnosed), i_*
(implementation-defined), u_* (undefined behaviour), warn_*. This imports
n_* and e_*:

- the test text is copied unchanged, and the loomcc directives are appended
  at the end (so every line number, and __LINE__, stays as mcpp wrote it);
- n_*: the expected output is `clang -E -P -std=c17` of the file, kept only
  if it matches every `dg-final ... grep` pattern mcpp gives (the standard's
  answer, as mcpp's author wrote it); the .expected sidecar holds it;
- e_*: each `dg-error` becomes a `loomcc-error` at the same line (or `@*`
  when mcpp says line 0), messages not checked (they are gcc's wording).

Tests that cannot be converted faithfully are listed and skipped.
"""
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "external/fetched/mcpp/cpp-test/test-t"
DST = ROOT / "tests/t1-pp/mcpp"
# Expected errors that are implementation choices here: `$` in identifiers
# is accepted (t1-pp/lex/dollar-identifier.c), so THIS$AND$THAT is a name.
NOT_ERRORS = {("e_18_4.c", 30): "`$` is an identifier character for loomcc (implementation-defined)"}
# Tests whose subject only exists before C99 (e.g. #if overflow of long).
C90_ONLY = {"e_14_10.c"}
SOURCE_NOTE = "mcpp 2.7.2 cpp-test/test-t/{name} (BSD-2-Clause, see LICENSE in this directory)"

DG_ERROR = re.compile(r'dg-error\s+"((?:[^"\\]|\\.)*)"(?:\s+"[^"]*"\s+\{\s*target\s+[^}]*\}\s+(\d+))?')
GREP = re.compile(r'\[grep\s+\S+\s+"((?:[^"\\]|\\.)*)"\]')


def tcl_to_py(p):
    # The patterns are Tcl regexps inside Tcl strings: undo the string escapes
    # for braces and brackets that Tcl needed.
    return p.replace("\\{", "{").replace("\\}", "}").replace("\\[", "[").replace("\\]", "]").replace("\\\\", "\\")


def main():
    if not SRC.exists():
        print("run external/fetch.sh mcpp first", file=sys.stderr)
        return 2
    DST.mkdir(parents=True, exist_ok=True)
    shutil.copy(SRC.parent / "LICENSE", DST / "LICENSE")
    for h in SRC.glob("*.h"):
        shutil.copy(h, DST / h.name)
    skipped, imported = [], []
    for src in sorted(SRC.glob("*.c")):
        name = src.name
        kind = name.split("_")[0]
        if kind not in ("n", "e"):
            continue
        text = src.read_text(encoding="latin-1")
        lines = text.split("\n")
        if re.search(r"^\s*#\s*include\s*<", text, re.M):
            skipped.append((name, "includes a hosted system header"))
            continue
        if name in C90_ONLY:
            skipped.append((name, "tests C90-only behaviour"))
            continue
        tail = ["", f"// loomcc-source: " + SOURCE_NOTE.format(name=name)]
        if "trigraph" in lines[0].lower() or re.search(r"\?\?[=/'()!<>-]", text):
            tail.append("// loomcc-xfail: trigraphs are deliberately unsupported (PLAN 2.1)")
        out_lines = list(lines)
        if kind == "e" or "dg-error" in text:
            n_err = 0
            bad = False
            for i, line in enumerate(lines):
                for m in DG_ERROR.finditer(line):
                    n_err += 1
                    at = m.group(2)
                    if at is None and (name, i + 1) in NOT_ERRORS:
                        tail.append(f"// loomcc-note: line {i + 1} is not an error here: " + NOT_ERRORS[(name, i + 1)])
                    elif at is None:
                        out_lines[i] = out_lines[i] + " // loomcc-error"
                    elif at == "0":
                        tail.append("// loomcc-error@*")
                    else:
                        tail.append(f"// loomcc-error@{int(at)}")
                if "dg-error" in line and not DG_ERROR.search(line):
                    bad = True
            if n_err == 0 or bad:
                skipped.append((name, "dg-error form not understood"))
                continue
            tail.insert(1, "// loomcc-do: preprocess")
            (DST / name).write_text("\n".join(out_lines).rstrip("\n") + "\n" + "\n".join(tail) + "\n", encoding="latin-1")
            imported.append(name)
            continue
        # n_*: expected output from clang, checked against mcpp's grep patterns.
        tail.insert(1, "// loomcc-do: preprocess")
        tail.append("// loomcc-note: expected output: clang -E -P -std=c17, checked against mcpp's dg-final patterns")
        dst = DST / name
        dst.write_text("\n".join(out_lines).rstrip("\n") + "\n" + "\n".join(tail) + "\n", encoding="latin-1")
        r = subprocess.run(["clang", "-E", "-P", "-std=c17", "-trigraphs", "-w", name], cwd=DST, capture_output=True, text=True)
        if r.returncode != 0:
            skipped.append((name, "clang -E fails: " + r.stderr.strip().splitlines()[0]))
            dst.unlink()
            continue
        patterns = [tcl_to_py(p) for p in GREP.findall(text)]
        missing = []
        for p in patterns:
            try:
                if not re.search(p, r.stdout, re.M):
                    missing.append(p)
            except re.error as e:
                missing.append(f"{p} (bad regex: {e})")
        if missing:
            skipped.append((name, "clang output misses mcpp pattern(s): " + "; ".join(missing)))
            dst.unlink()
            continue
        dst.with_suffix(".expected").write_text(r.stdout)
        imported.append(name)
    print(f"imported {len(imported)} tests into {DST.relative_to(ROOT)}")
    for n, why in skipped:
        print(f"skipped {n}: {why}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
