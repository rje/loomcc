#!/usr/bin/env python3
"""Selects the GCC torture execute tests that apply to loomcc at 16-bit int.

    external/fetch.sh gcc-torture
    external/classify-gcc-torture.py            # static filter + host16 run, writes the list
    external/classify-gcc-torture.py --static   # static filter only

GCC's tests are GPL-3.0-or-later: they are never copied into this repository.
This script reads them from external/fetched/gcc, and writes

- external/gcc-torture-execute.list: the selected test names (facts about
  the tests, not their content), one per line, with the reason for every
  rejected test in external/gcc-torture-execute.rejected;
- external/fetched/gcc-wrapped/<name>.c: one runner test per selected test,
  which #includes the original (ignored by git, regenerated on demand).

Static filter: no floating point, no `long long`, no GNU extensions the
suite does not test (attributes, statement expressions, nested functions,
builtins other than abort/exit, asm, typeof, case ranges, labels as values,
vector types), no system headers beyond the declarations of abort/exit, no
dg- directives that need options or targets. Dynamic filter: the test must
pass under host16 (clang's msp430 front end: 16-bit int, 32-bit long, run by
lli), i.e. it does not assume a 32-bit int.
"""
import os
import re
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "external/fetched/gcc/gcc/testsuite/gcc.c-torture/execute"
WRAP = ROOT / "external/fetched/gcc-wrapped"
def _llvm_bin():
    """$LLVM_BIN, else the directory of lli on PATH, else Homebrew's LLVM."""
    if os.environ.get("LLVM_BIN"):
        return Path(os.environ["LLVM_BIN"])
    lli = shutil.which("lli")
    return Path(lli).parent if lli else Path("/opt/homebrew/opt/llvm/bin")


LLVM = _llvm_bin()
HARNESS = ROOT / "harness/include"

REJECT = [
    (r"\b(float|double|_Complex|__complex__|_Float\d+|_Decimal)\b", "floating point or complex"),
    (r"\blong\s+long\b|\bLL\b|[0-9a-fA-F][uU]?[lL][lL]\b|\bint64_t\b|__int128", "64-bit integers"),
    (r"\b\d+\.\d*(?:[eE][+-]?\d+)?[fFlL]?\b|\b\d+[eE][+-]?\d+[fFlL]?\b|(?<![\w.])\.\d+", "floating constants"),
    (r"__[A-Z0-9_]+_TYPE__|__[A-Z0-9_]+_(?:MAX|MIN|WIDTH)__", "compiler-predefined type macros"),
    (r"\bstruct\s+\w*\s*\{\s*\}", "empty structs (GNU)"),
    (r"\(\s*\{", "statement expressions"),
    (r"__builtin_(?!abort\b|exit\b)", "builtins"),
    (r"\b(asm|__asm__|__asm)\b", "inline assembly"),
    (r"\b(typeof|__typeof__|__typeof)\b", "typeof"),
    (r"\bcase\b[^:\n]*\.\.\.", "case ranges"),
    (r"&&\s*[A-Za-z_]\w*\s*[;,)]|goto\s*\*", "labels as values"),
    (r"vector_size|__vector", "vector types"),
    (r"__label__|__extension__|__alignof__|__restrict__|__inline__", "GNU keywords"),
    (r"\b(isprint|isdigit|isalpha|isspace|toupper|tolower|abs|labs|atoi|strtol|qsort|bsearch|printf|sprintf|puts|fprintf|malloc|calloc|free|realloc|alloca|memcpy|memset|memcmp|memmove|strcmp|strcpy|strlen|strncmp|strcat|setjmp|longjmp|va_start|va_arg|signal|__FUNCTION__)\b", "library functions"),
    (r"dg-(options|additional-options|add-options|require|skip-if|xfail|do|timeout|prune)", "dg directives"),
    (r"\bwchar_t\b|\bL'|\bL\"", "wide characters"),
    (r"#\s*pragma", "pragmas"),
    (r"\bthread\b|_Thread_local|__thread|_Atomic", "threads/atomics"),
    (r"\bsizeof\s*\(\s*(int|long|void\s*\*)\s*\)\s*[<>=!]=?\s*\d", "checks type sizes"),
    (r"__INT_MAX__|__LONG_MAX__|__SIZEOF_|__CHAR_BIT__|INT_MAX|UINT_MAX|LONG_MAX", "width macros"),
]
ALLOWED_INCLUDES = {"stdlib.h"}
# Checked by hand: outside what loomcc implements, though the filters miss them.
EXCLUDE = {
    "970217-1.c": "variably modified parameter (VLA; C17 optional, loomcc has none)",
    "pr22061-2.c": "variably modified type (VLA)",
    "20010924-1.c": "static initialisation of a flexible array member (GNU)",
}


# Attributes that only steer optimisation or warnings: the wrappers define
# __attribute__(x) away, which keeps the test's meaning.
HARMLESS_ATTRS = {"noinline", "noclone", "noipa", "unused", "used", "noreturn", "const", "pure",
                  "always_inline", "cold", "hot", "nonnull", "malloc", "warn_unused_result", "leaf", "nothrow"}


def attributes_ok(text):
    for m in re.finditer(r"__attribute(?:__)?\s*\(\((.*?)\)\)", text, re.S):
        words = re.findall(r"[A-Za-z_]\w*", re.sub(r"\(.*?\)", "", m.group(1)))
        for w in words:
            if w.strip("_") not in HARMLESS_ATTRS:
                return False
    return True


def static_reason(text):
    if re.search(r"__attribute", text) and not attributes_ok(text):
        return "GNU attributes that change meaning"
    for m in re.finditer(r"^\s*#\s*include\s*[<\"]([^>\"]+)[>\"]", text, re.M):
        if m.group(1) not in ALLOWED_INCLUDES:
            return f"includes {m.group(1)}"
    for pat, why in REJECT:
        if re.search(pat, text):
            return why
    return None


def host16(path):
    """True when the test passes under clang --target=msp430 + lli."""
    work = WRAP / "_host16"
    work.mkdir(parents=True, exist_ok=True)
    ll = work / (path.stem + ".ll")
    r = subprocess.run(["taskpolicy", "-b", "nice", "-n", "19", str(LLVM / "clang"), "--target=msp430-none-elf", "-fsigned-char", "-std=gnu17",
                        "-O0", "-w", "-S", "-emit-llvm", "-D__builtin_abort=abort", "-D__builtin_exit=exit", "-D__attribute__(x)=",
                        str(path), "-o", str(ll)], capture_output=True, text=True)
    if r.returncode != 0:
        return "msp430 compile fails: " + (r.stderr.strip().splitlines() or ["?"])[-1][:100]
    text = ll.read_text().replace("p:16:16", "p:64:64").replace(" optnone", "")
    ll.write_text(text)
    opt = work / (path.stem + ".opt.ll")
    r = subprocess.run(["taskpolicy", "-b", "nice", "-n", "19", str(LLVM / "opt"), "-S", "-passes=instcombine<no-verify-fixpoint>", str(ll), "-o", str(opt)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return "opt fails"
    text = re.sub(r"= freeze (\S+) (.+)$", r"= bitcast \1 \2 to \1", opt.read_text(), flags=re.M)
    opt.write_text(text)
    try:
        r = subprocess.run(["taskpolicy", "-b", "nice", "-n", "19", str(LLVM / "lli"), "-force-interpreter", str(opt)], capture_output=True, text=True, timeout=60)
    except subprocess.TimeoutExpired:
        return "host16 timeout"
    return None if r.returncode == 0 else f"fails at 16-bit int (lli exit {r.returncode})"


def wrap(path):
    WRAP.mkdir(parents=True, exist_ok=True)
    (WRAP / path.name).write_text(
        "// loomcc-do: run\n"
        "// loomcc-int: 16\n"
        "// loomcc-max-frames: 3600\n"
        "// loomcc-timeout: 120\n"
        "// loomcc-options: -D__builtin_abort=abort -D__builtin_exit=exit '-D__attribute__(x)='\n"
        f"// loomcc-source: GCC gcc/testsuite/gcc.c-torture/execute/{path.name} (GPL-3.0-or-later; fetched, not vendored)\n"
        f'#include "{path}"\n')


def main():
    if not SRC.exists():
        print("run external/fetch.sh gcc-torture first", file=sys.stderr)
        return 2
    static_only = "--static" in sys.argv
    tests = sorted(SRC.glob("*.c"))
    rejected, candidates = {}, []
    for p in tests:
        why = EXCLUDE.get(p.name) or static_reason(p.read_text(errors="replace"))
        if why:
            rejected[p.name] = why
        else:
            candidates.append(p)
    print(f"{len(tests)} tests, {len(candidates)} pass the static filter")
    selected = candidates
    if not static_only:
        with ThreadPoolExecutor(max_workers=3) as ex:
            results = list(ex.map(host16, candidates))
        selected = []
        for p, why in zip(candidates, results):
            if why:
                rejected[p.name] = why
            else:
                selected.append(p)
        print(f"{len(selected)} pass under host16 (16-bit int)")
    (ROOT / "external/gcc-torture-execute.list").write_text(
        "# GCC gcc.c-torture/execute tests that apply at 16-bit int (external/classify-gcc-torture.py)\n"
        + "".join(p.name + "\n" for p in selected))
    (ROOT / "external/gcc-torture-execute.rejected").write_text(
        "# rejected GCC torture execute tests and why (external/classify-gcc-torture.py)\n"
        + "".join(f"{n}\t{w}\n" for n, w in sorted(rejected.items())))
    for old in WRAP.glob("*.c"):
        old.unlink()
    for p in selected:
        wrap(p)
    return 0


if __name__ == "__main__":
    sys.exit(main())
