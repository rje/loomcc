#!/usr/bin/env python3
"""Reduces a failing T7 program with C-Reduce (brew install creduce).

    tests/t7-random/reduce.py FAILING.c --mode ir|rom [--out reduced.c]

FAILING.c is a runner test written by run.py (gen.py or --csmith). The
interestingness test keeps a candidate only when

1. clang --target=msp430 (16-bit int) accepts it with the warnings that
   usually mean C-Reduce introduced undefined behaviour made errors
   (uninitialised use, missing return, implicit declarations, ...);
2. host16 runs it to completion and gives the same checksum C whether
   uninitialised locals start as zeros or as a pattern (a read of an
   uninitialised local is the UB C-Reduce most often introduces);
3. loomcc (the chosen mode) builds it, runs it with -DEXPECTED=C and
   aborts: it still disagrees with host16 (a compile error does not count).

The reduced program is written with the runner header, with EXPECTED set
from host16, ready to become a permanent T4 test.
"""
import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE))
from run import host16_checksum, CSMITH_DIR  # noqa: E402

UB_WARNINGS = ["-Werror=uninitialized", "-Werror=sometimes-uninitialized", "-Werror=return-type",
               "-Werror=implicit-function-declaration", "-Werror=implicit-int", "-Werror=array-bounds",
               "-Werror=division-by-zero", "-Werror=shift-count-overflow", "-Werror=shift-count-negative",
               "-Werror=int-conversion", "-Werror=incompatible-pointer-types", "-Werror=unsequenced",
               "-Werror=format-insufficient-args", "-Werror=format", "-Werror=c23-extensions",
               "-Werror=gnu-empty-initializer", "-Werror=missing-braces", "-Werror=empty-body",
               "-Werror=tentative-definition-array"]
# Scaffolding a candidate must keep (else C-Reduce "reduces" the checksum away).
KEEP_GEN = [r'#include "loomcc-test.h"', r'printf("%u\n", (unsigned)checksum());', r"return checksum() != EXPECTED;"]
KEEP_CSMITH = [r'#include "csmith.h"', r"platform_main_end("]


def body_of(test_text):
    """The program without the runner's // loomcc- header lines."""
    return "".join(l for l in test_text.splitlines(True) if not l.startswith("// loomcc-"))


def main(argv):
    if len(argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    src = Path(argv[1]).resolve()
    mode = argv[argv.index("--mode") + 1] if "--mode" in argv else "ir"
    out = Path(argv[argv.index("--out") + 1]) if "--out" in argv else src.with_name(src.stem + "-reduced.c")
    text = src.read_text()
    csmith = "csmith" in text.split("\n", 6)[3] or "-I%ROOT%/tests/t7-random/csmith" in text
    work = Path(tempfile.mkdtemp(prefix="loomcc-reduce-"))
    prog = work / "prog.c"
    prog.write_text(body_of(text))
    extra = [f"-I{CSMITH_DIR}"] if csmith else []
    # Keep the test's other -D options (e.g. --shapes --no-foreign's -DT7_NO_FOREIGN).
    om = re.search(r"^// loomcc-options:(.*)$", text, re.M)
    extra += [o for o in (om.group(1).split() if om else []) if o.startswith("-D") and not o.startswith("-DEXPECTED=")]
    check = work / "interesting.py"
    keep = KEEP_CSMITH if csmith else KEEP_GEN
    check.write_text(f"""#!/usr/bin/env python3
import subprocess, sys
sys.path.insert(0, {str(HERE)!r})
from run import host16_checksum
from pathlib import Path
extra = {extra!r}
p = Path("prog.c")
text = p.read_text()
if not all(k in text for k in {keep!r}):
    sys.exit(1)
r = subprocess.run(["clang", "--target=msp430-none-elf", "-fsigned-char", "-std=c17", "-pedantic-errors", "-Wno-compare-distinct-pointer-types", "-fsyntax-only",
                    "-I{ROOT / 'harness/include'}", "-DLOOMCC_T7_PRINT=1", *extra, *{UB_WARNINGS!r}, "prog.c"],
                   capture_output=True)
if r.returncode:
    sys.exit(1)
try:
    # Reading an uninitialised local is undefined: the checksum must not
    # depend on what uninitialised automatic storage holds.
    ck = host16_checksum(p, Path("."), extra, "zero")
    if host16_checksum(p, Path("."), extra, "pattern") != ck:
        sys.exit(1)
except Exception:
    sys.exit(1)
t = Path("cand.c")
opts = " ".join(["-DEXPECTED=%du" % ck] + [e.replace({str(CSMITH_DIR)!r}, "%ROOT%/tests/t7-random/csmith") for e in extra])
t.write_text("// loomcc-do: run\\n// loomcc-int: 16\\n// loomcc-options: " + opts + "\\n// loomcc-ref:\\n" + p.read_text())
r = subprocess.run([{str(ROOT / 'run-tests')!r}, "--modes", {mode!r}, "--work", "rw", str(t.resolve())], capture_output=True, text=True)
# Interesting only if loomcc built and ran it and got a different checksum
# (an abort or a failed CHECK), not if it rejected the program.
bad = " 1 FAIL" in r.stdout and ("abort() called" in r.stdout or "CHECK failed" in r.stdout or "main returned" in r.stdout)
sys.exit(0 if bad else 1)
""")
    check.chmod(check.stat().st_mode | stat.S_IEXEC)
    first = subprocess.run([str(check)], cwd=work)
    if first.returncode != 0:
        print("the program is not interesting to begin with (loomcc agrees with host16, or host16 rejects it)")
        return 1
    subprocess.run(["taskpolicy", "-b", "nice", "-n", "19", "creduce", "--n", "1" if mode == "rom" else "2", str(check), "prog.c"], cwd=work)
    ck = host16_checksum(work / "prog.c", work, extra)
    header = (f"// loomcc-do: run\n// loomcc-int: 16\n// loomcc-options: -DEXPECTED={ck}u"
              + (" -I%ROOT%/tests/t7-random/csmith" if csmith else "")
              + "".join(" " + e for e in extra if e.startswith("-D")) + "\n"
              f"// loomcc-note: reduced by tests/t7-random/reduce.py from {src.name}; loomcc {mode} disagreed with host16\n")
    out.write_text(header + (work / "prog.c").read_text())
    print(f"reduced program: {out}")
    shutil.rmtree(work, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
