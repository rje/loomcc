#!/usr/bin/env python3
"""Writes link stubs for a T6 stage-2 driver.

    scripts/t6-stubs.py tests/t6-loom/run/<driver>.c

A driver #includes one Loom runtime unit, which references functions and
data of other units and of Loom's assembly. The driver never calls those,
but the ROM must still link. This builds the driver with 816-tcc and with
loomcc, collects every "unknown label" wlalink reports, and writes
<driver>.stubs.asm: a RAM label per symbol (never executed, never read by
the functions under test). The driver names the file in loomcc-asm-sources.
Rerun it when the driver or the copied Loom sources change.
"""
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def unknown_labels(test, mode):
    work = Path(tempfile.mkdtemp(prefix="t6-stubs-"))
    args = [str(ROOT / "run-tests"), "-vv", "-j", "1", "--work", str(work)]
    args += ["--refs", "tcc-rom", "--refs-only"] if mode == "tcc-rom" else ["--modes", mode]
    r = subprocess.run(args + [str(test)], capture_output=True, text=True)
    shutil.rmtree(work, ignore_errors=True)
    out = r.stdout + r.stderr
    # wlalink: 'Reference to an unknown label "x"', or for a label used in a
    # computed operand, 'PARSE_STACK: Unresolved reference to "x"'.
    return set(re.findall(r'(?:Reference to an unknown label|Unresolved reference to) "([^"]+)"', out))


def main(argv):
    if len(argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    test = Path(argv[1]).resolve()
    stubs = test.with_suffix(".stubs.asm")
    names = set()
    if stubs.exists():
        names |= set(re.findall(r"^(\w+) dsb", stubs.read_text(), re.M))
    for _ in range(16):
        text = (".include \"hdr.asm\"\n; Link stubs for " + test.name + " (scripts/t6-stubs.py): symbols the\n"
                "; included runtime unit references but the driver never reaches.\n"
                ".BASE $00\n.RAMSECTION \"t6_stubs_" + re.sub(r"\W", "_", test.stem) + "\" BANK $7E SLOT 2\n"
                + "".join(f"{n} dsb 4\n" for n in sorted(names)) + ".ENDS\n")
        stubs.write_text(text)
        found = unknown_labels(test, "tcc-rom") | unknown_labels(test, "rom")
        if not found - names:
            break
        names |= found
    print(f"{stubs.name}: {len(names)} stubs")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
