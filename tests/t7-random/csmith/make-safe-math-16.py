#!/usr/bin/env python3
"""Regenerates safe_math_16.h from Csmith's safe_math.h: keeps the 8/16/32-bit
integer wrappers, drops the 64-bit and floating-point ones (loomcc's subset).

    tests/t7-random/csmith/make-safe-math-16.py [path/to/csmith/safe_math.h]
"""
import sys
from pathlib import Path

src = Path(sys.argv[1] if len(sys.argv) > 1 else "/opt/homebrew/Cellar/csmith/2.3.0/include/csmith-2.3.0/safe_math.h").read_text()
head, _, rest = src.partition("\nSTATIC ")
blocks = rest.split("\nSTATIC ")
keep = ["STATIC " + b.rstrip() for b in blocks if not any(x in b.split("{", 1)[0] for x in ("int64", "float", "double"))]
lines = [l for l in (head + "\n" + "\n\n".join(keep)).split("\n") if l.strip() != "#ifndef NO_LONGLONG"]
out, depth = [], 0
for l in lines:
    s = l.strip()
    if s.startswith("#if"):
        depth += 1
    elif s.startswith("#endif"):
        if depth <= 1:      # would close the include guard early: an orphan
            continue
        depth -= 1
    out.append(l)
text = ("/* safe_math_16.h: Csmith 2.3.0's safe_math.h (Copyright The University of\n"
        " * Utah; BSD licence, see LICENSE.csmith) without the 64-bit and floating-point\n"
        " * wrappers; regenerate with make-safe-math-16.py. Csmith's\n"
        " * #if (INTn_MAX >= INT_MAX) guards make every wrapper overflow-safe at\n"
        " * 16-bit int. */\n" + "\n".join(out) + "\n#endif\n")
Path(__file__).with_name("safe_math_16.h").write_text(text)
print(f"{len(keep)} wrappers kept")
