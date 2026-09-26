#!/usr/bin/env python3
"""Wraps GCC's gcc.dg compile tests that expect errors as T3 constraint tests.

    external/fetch.sh gcc-dg && external/wrap-gcc-dg-errors.py

GPL-3.0-or-later: the tests are never copied; wrappers in
external/fetched/gcc-dg-errors-wrapped/ #include them (by absolute path, so
diagnostics carry the original file's line numbers).

Selected: top-level gcc.dg/*.c with `dg-do compile` (or no dg-do), at least
one dg-error, and dg-options (if any) drawn only from standard-C switches
(-std=c99/c11/c17 and their iso9899 spellings, -pedantic, -pedantic-errors)
and switches that change no diagnostics here (-O*, -g, -Wall, -Wextra, -w).
Each dg-error becomes `loomcc-diagnostic@<file>:<line>`: C17 requires a
diagnostic for a constraint violation, and whether it is an error is the
compiler's choice (GCC's own choice changed in GCC 14); the message regex is not carried over (compilers word things
differently). dg-warning, dg-message and dg-bogus lines are ignored.
Dropped statically: target selectors or xfails on a dg-error, relative line
forms other than `.`/`.+N`/`.-N`/`N`, GNU extensions (attributes, builtins,
asm, statement expressions, typeof, __int128, nested functions via
`auto`), floating point, _Complex/_Atomic/_Thread_local/_BitInt, pragmas,
and system headers other than the ones loomcc ships.
Dropped dynamically: clang (msp430) with -pedantic-errors -Werror=vla
must report errors only at expected lines (otherwise the test's accepted
parts rely on GNU extensions or VLAs, which loomcc rejects by design); the clang16 reference (clang --target=msp430,
16-bit int, -std=c17 -pedantic) must report every expected diagnostic at
its line and no error elsewhere; clang is the oracle, GCC the source.
"""
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "external/fetched/gcc/gcc/testsuite/gcc.dg"
OUT = ROOT / "external/fetched/gcc-dg-errors-wrapped"
STD_OPTS = {"-std=c99", "-std=c11", "-std=c17", "-std=c18", "-std=iso9899:1999", "-std=iso9899:2011",
            "-std=iso9899:2017", "-std=iso9899:2018", "-pedantic", "-pedantic-errors"}
NEUTRAL = re.compile(r"^(-O[0-3sg]?|-g\d?|-Wall|-Wextra|-w)$")
HEADERS = {"assert.h", "limits.h", "stdarg.h", "stdbool.h", "stddef.h", "stdint.h", "stdio.h", "stdlib.h", "string.h"}
GNU = re.compile(r"__attribute__|__builtin|\basm\b|__asm|\(\{|\btypeof\b|__typeof|__int128|__extension__|__label__|"
                 r"\b(float|double)\b|_Complex|_Imaginary|_Atomic|_Thread_local|_BitInt|_Decimal|_Float\d|__auto_type|"
                 r"#\s*pragma|_Pragma|\bconstexpr\b|\bnullptr\b|\bauto\s+\w+\s*\(|__seg_|__restrict|__inline|__const|"
                 r"__signed|__volatile|__alignof|__real|__imag|\bthread_local\b|\bstatic_assert\b|\bbool\b|\btrue\b|\bfalse\b|"
                 r"\balignas\b|\balignof\b|\btypeof_unqual\b|#\s*embed|__has_")
# Tests whose accepted parts are not strictly conforming C17, found by hand.
HAND = {"pr66618-2.c": "accepts `\"foo\"[2]` as a constant initializer, which C17 6.6 does not require (6.6p10 allows it)"}
DG_ERR = re.compile(r'dg-error\s+(?:"((?:[^"\\]|\\.)*)"|\{[^}]*\}|\S+)(.*)$')


def static(src, text):
    if re.search(r"dg-do\s+(run|link|assemble|preprocess)", text):
        return "not a compile test"
    if "dg-error" not in text:
        return "no dg-error"
    if re.search(r"dg-(additional-options|require|skip-if|xfail-if|add-options|additional-sources|final|ice)", text):
        return "requirements, extra options or dg-final checks"
    opts = " ".join(re.findall(r'dg-options\s+"([^"]*)"', text) + re.findall(r'dg-options\s+(-\S+)', text)).split()
    if re.search(r'dg-options\s+"[^"]*"\s*\{', text):
        return "target-specific options"
    if any(o not in STD_OPTS and not NEUTRAL.match(o) for o in opts):
        return "options"
    for h in re.findall(r'#\s*include\s*[<"]([^>"]+)[>"]', text):
        if h not in HEADERS:
            return f"header {h}"
    m = GNU.search(text)
    if m:
        return f"GNU or unsupported feature ({m.group(0).strip()})"
    return None, opts


def directives(src, text, pedantic_errors):
    out = []
    for i, line in enumerate(text.split("\n"), 1):
        if "dg-error" not in line:
            continue
        m = DG_ERR.search(line)
        if not m:
            return None
        rest = m.group(2)
        at = i
        # dg-error "msg" "comment" { target } line  (every part optional)
        tail = re.sub(r'^\s*"(?:[^"\\]|\\.)*"', "", rest).strip()
        tm = re.match(r'\{\s*([^}]*)\}\s*(\S*)\s*\}', tail)
        if tm:
            sel, ln = tm.group(1).strip(), tm.group(2)
            if sel and sel not in ("target *-*-*", ""):
                return None
        else:
            ln = ""
            if not re.match(r"^\}", tail):
                return None
        if ln:
            if re.fullmatch(r"\d+", ln):
                at = int(ln)
            elif re.fullmatch(r"\.([+-]\d+)?", ln):
                at = i + int(ln[1:] or 0)
            else:
                return None
        # C17 5.1.1.3 requires a diagnostic for a constraint violation, not
        # an error: GCC's dg-error is GCC's choice (GCC 14 made implicit
        # declarations and int/pointer conversions errors; earlier versions
        # and loomcc warn). So every dg-error becomes loomcc-diagnostic.
        out.append(f"// loomcc-diagnostic@{src}:{at}")
    return sorted(set(out))


def main():
    if OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir(parents=True)
    rejected = {}
    for src in sorted(SRC.glob("*.c")):
        text = src.read_text(errors="replace")
        if src.name in HAND:
            rejected[src.name] = HAND[src.name]
            continue
        why = static(src, text)
        if not isinstance(why, tuple):
            if why != "no dg-error" and why != "not a compile test":
                rejected[src.name] = why
            continue
        opts = why[1]
        ds = directives(src, text, "-pedantic-errors" in opts)
        if not ds:
            rejected[src.name] = "dg-error form not converted (target selector, xfail or line spec)"
            continue
        lines = {int(d.rsplit(":", 1)[1]) for d in ds}
        r = subprocess.run(["taskpolicy", "-b", "nice", "-n", "19", "clang", "--target=msp430-none-elf", "-fsigned-char", "-std=c17",
                            "-pedantic-errors", "-Werror=vla", "-fsyntax-only", "-fno-caret-diagnostics", str(src)],
                           capture_output=True, text=True, cwd=OUT)
        stray = [int(m.group(1)) for m in re.finditer(re.escape(src.name) + r":(\d+):\d+: error:", r.stderr)
                 if int(m.group(1)) not in lines]
        if stray:
            rejected[src.name] = f"relies on extensions or VLAs (clang -pedantic-errors -Werror=vla errors at line {stray[0]})"
            continue
        (OUT / src.name).write_text(
            "// loomcc-do: syntax\n// loomcc-ref: clang16\n"
            f"// loomcc-source: GCC gcc/testsuite/gcc.dg/{src.name} (GPL-3.0-or-later; fetched, not vendored)\n"
            + "\n".join(ds) + f'\n#include "{src}"\n')
    js = OUT / "_refs.json"
    subprocess.run([str(ROOT / "run-tests"), "--refs-only", "-j", "2", "--json", str(js), str(OUT)], stdout=subprocess.DEVNULL)
    for line in js.read_text().splitlines():
        r = json.loads(line)
        if r["status"] != "PASS":
            name = r["test"].split("/")[-1]
            rejected[name] = "clang disagrees: " + (r["detail"].splitlines() or ["?"])[0][:140]
            (OUT / name).unlink(missing_ok=True)
    js.unlink()
    names = sorted(p.name for p in OUT.glob("*.c"))
    (ROOT / "external/gcc-dg-errors.list").write_text(
        "# gcc.dg tests wrapped as T3 constraint tests by external/wrap-gcc-dg-errors.py\n" + "".join(n + "\n" for n in names))
    (ROOT / "external/gcc-dg-errors.rejected").write_text(
        "# gcc.dg compile tests with dg-error lines not wrapped, and why\n" + "".join(f"{n}\t{w}\n" for n, w in sorted(rejected.items())))
    print(f"{len(names)} gcc.dg error tests wrapped ({len(rejected)} rejected); run with ./run-tests external/fetched/gcc-dg-errors-wrapped")
    return 0


if __name__ == "__main__":
    sys.exit(main())
