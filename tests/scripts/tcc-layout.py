#!/usr/bin/env python3
"""Asks 816-tcc for layout facts: sizeof/offsetof/_Alignof-like expressions.

    scripts/tcc-layout.py prelude.c 'sizeof(struct A)' 'offsetof(struct A, p)' ...
    scripts/tcc-layout.py prelude.c -       # expressions one per line on stdin

The prelude (declarations) and a `const unsigned short probe[]` table of the
expressions are compiled with `816-tcc -c`; the table's bytes are read back
from the generated assembly (`probe: .db ...`) and printed as
`<value>\t<expression>`. This is how the T3 layout expectations are pinned.
"""
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

PVS = Path(os.environ.get("PVSNESLIB_HOME", Path.home() / "Library/Loom/Toolchains/v0/artifacts/pvsneslib"))
TCC = PVS / "devkitsnes/bin/816-tcc"
HARNESS = Path(__file__).resolve().parent.parent / "harness/include"


def main(argv):
    if len(argv) < 3:
        print(__doc__, file=sys.stderr)
        return 2
    prelude = Path(argv[1]).read_text()
    exprs = [l.strip() for l in sys.stdin] if argv[2] == "-" else argv[2:]
    exprs = [e for e in exprs if e]
    with tempfile.TemporaryDirectory() as d:
        src = Path(d) / "probe.c"
        src.write_text("#include <stddef.h>\n" + prelude + "\nconst unsigned short probe[] = {\n"
                       + ",\n".join(f"  (unsigned short)({e})" for e in exprs) + "\n};\n")
        out = Path(d) / "probe.ps"
        r = subprocess.run(["taskpolicy", "-b", "nice", "-n", "19", str(TCC), f"-I{PVS / 'devkitsnes/include'}", f"-I{HARNESS}",
                            "-c", str(src), "-o", str(out)], capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stdout + r.stderr, file=sys.stderr)
            return 1
        text = out.read_text()
    m = re.search(r"^probe:\s*\.db\s+(.*)$", text, re.M)
    if not m:
        print("no probe table in 816-tcc output", file=sys.stderr)
        return 1
    b = [int(x.strip().lstrip("$"), 16) for x in m.group(1).split(",")]
    for i, e in enumerate(exprs):
        print(f"{b[2 * i] | (b[2 * i + 1] << 8)}\t{e}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
