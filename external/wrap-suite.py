#!/usr/bin/env python3
"""Wraps a fetched external suite as runner tests, keeping those that apply.

    external/wrap-suite.py tcc-tests2 | llvm-unittests | llvm-regression | chibicc | c-testsuite

The suites are fetched by external/fetch.sh and are never copied into this
repository (tcc's tests are LGPL; the others keep their own licences, see
docs/SOURCES.md). This script writes one wrapper per candidate into
external/fetched/wrapped-<suite>/, which #includes the original and states
its expected output (loomcc-expect-output), then keeps only the wrappers
that pass both

- a static filter (no floating point, 64-bit integers, GNU extensions,
  library functions beyond what the harness provides: printf, puts,
  putchar, abort, exit, and PVSnesLib's mem*/str*), and
- a run of the reference tools: host clang (the expected output is right
  for this wrapper) and 816-tcc's harness ROM (the test holds with 16-bit
  int). The 816-tcc filter is conservative: a test 816-tcc miscompiles is
  dropped too. host16 cannot run printing tests.

The selected names go to external/<suite>.list (committed; names only).
"""
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FETCHED = ROOT / "external/fetched"
LIBC = ROOT / "harness/libc"

REJECT = [
    (r"\b(float|double|_Complex|long\s+double)\b", "floating point"),
    (r"\b\d+\.\d*(?:[eE][+-]?\d+)?[fFlL]?\b|\b\d+[eE][+-]?\d+[fFlL]?\b", "floating constants"),
    (r"\blong\s+long\b|[0-9a-fA-F][uU]?[lL][lL]\b|int64_t|uint64_t|__int128", "64-bit integers"),
    (r"__attribute__|\(\s*\{|__builtin_|\basm\b|__asm__|\btypeof\b|__typeof__|__label__", "GNU extensions"),
    (r"\b(malloc|calloc|realloc|free|sprintf|snprintf|fprintf|fputs|fopen|fclose|fgets|scanf|sscanf|strtol|atoi|qsort|setjmp|longjmp|signal|time|clock|rand|srand|getchar|fflush|stderr|stdout|va_start|alloca|strcat|strncpy|strchr|strrchr|strstr|isdigit|isalpha|isspace|toupper|tolower|abs|labs)\b", "library functions"),
    (r"\b(wchar_t|_Thread_local|_Atomic|_Alignas|_Generic)\b|\bL'|\bL\"", "wide/C11 features"),
    (r"#\s*pragma", "pragmas"),
]
ALLOWED_INCLUDES = {"stdio.h", "stdlib.h", "string.h"}
# Checked by hand: not C17 (implicit int in K&R definitions).
EXCLUDE = {"DuffsDevice.c": "implicit int (C89 only)", "2002-12-13-MishaTest.c": "implicit int (C89 only)"}


def static_reason(text):
    for m in re.finditer(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', text, re.M):
        if m.group(1) not in ALLOWED_INCLUDES and not m.group(1).endswith("test.h"):
            return f"includes {m.group(1)}"
    for pat, why in REJECT:
        if re.search(pat, text):
            return why
    return None


def suites():
    t2 = FETCHED / "tinycc/tests/tests2"
    uni = FETCHED / "llvm-test-suite/SingleSource/UnitTests"
    reg = FETCHED / "llvm-test-suite/SingleSource/Regression/C"
    return {
        "tcc-tests2": [(p, p.with_suffix(".expect"), 0) for p in sorted(t2.glob("*.c")) if p.with_suffix(".expect").exists()],
        "llvm-unittests": [(p, p.with_suffix(".reference_output"), None) for p in sorted(uni.glob("*.c")) if p.with_suffix(".reference_output").exists()],
        "llvm-regression": [(p, p.with_suffix(".reference_output"), None) for p in sorted(reg.glob("*.c")) if p.with_suffix(".reference_output").exists()],
        "c-testsuite": [(p, Path(str(p) + ".expected"), 0) for p in sorted((FETCHED / "c-testsuite/tests/single-exec").glob("*.c"))],
        "chibicc": [(p, None, 0) for p in sorted((FETCHED / "chibicc/test").glob("*.c"))],
    }


def main(argv):
    if len(argv) != 2 or argv[1] not in suites():
        print(__doc__, file=sys.stderr)
        return 2
    name = argv[1]
    out = FETCHED / f"wrapped-{name}"
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    rejected = {}
    candidates = []
    for src, expect, _ in suites()[name]:
        text = src.read_text(errors="replace")
        why = EXCLUDE.get(src.name) or static_reason(text)
        if why:
            rejected[src.name] = why
            continue
        lines = ["// loomcc-do: run", "// loomcc-int: 16", f"// loomcc-options: -I{LIBC}",
                 "// loomcc-max-frames: 3600", "// loomcc-timeout: 120",
                 f"// loomcc-source: {src.relative_to(FETCHED)} (fetched, not vendored; docs/SOURCES.md)"]
        if expect is not None:
            data = expect.read_bytes()
            if name.startswith("llvm"):
                # LLVM reference outputs end with "exit N".
                m = re.search(rb"\n?exit (\d+)\n?$", data)
                if not m or m.group(1) != b"0":
                    rejected[src.name] = "expects a non-zero exit"
                    continue
                data = data[: m.start()] + (b"\n" if m.group(0).startswith(b"\n") and data[: m.start()] else b"")
            exp = out / (src.stem + ".expected-output")
            exp.write_bytes(data)
            lines.append(f"// loomcc-expect-output: {exp.name}")
        if name == "chibicc":
            common = out / "chibicc-common.c"
            if not common.exists():
                common.write_text(f'#include "{FETCHED / "chibicc/test/common"}"\n')
            lines.append("// loomcc-extra-sources: chibicc-common.c")
            lines.append(f"// loomcc-options: -I{FETCHED / 'chibicc/test'}")
        lines.append(f'#include "{src}"')
        w = out / src.name
        w.write_text("\n".join(lines) + "\n")
        candidates.append(w)
    print(f"{name}: {len(candidates)} of {len(suites()[name])} pass the static filter")
    js = out / "_refs.json"
    subprocess.run([str(ROOT / "run-tests"), "--refs-only", "--refs", "host,tcc-rom", "-j", "3", "--json", str(js), str(out)],
                   stdout=subprocess.DEVNULL)
    status = {}
    for line in js.read_text().splitlines():
        r = json.loads(line)
        status.setdefault(r["test"], {})[r["mode"]] = (r["status"], r["detail"].splitlines()[0] if r["detail"] else "")
    selected = []
    for w in candidates:
        key = f"ext-wrapped-{name}/{w.name}"
        st = status.get(key, {})
        bad = [f"{m[4:]}: {d}" for m, (s, d) in st.items() if s != "PASS" and s != "UNSUPPORTED"]
        if bad:
            rejected[w.name] = "; ".join(bad)[:160]
            w.unlink()
        else:
            selected.append(w.name)
    js.unlink()
    (ROOT / f"external/{name}.list").write_text(
        f"# {name} tests selected by external/wrap-suite.py (static filter, host clang and 816-tcc ROM pass)\n"
        + "".join(n + "\n" for n in selected))
    (ROOT / f"external/{name}.rejected").write_text(
        f"# {name} tests rejected by external/wrap-suite.py, with reasons\n"
        + "".join(f"{n}\t{w}\n" for n, w in sorted(rejected.items())))
    print(f"{name}: {len(selected)} selected; run with ./run-tests external/fetched/wrapped-{name}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
