#!/usr/bin/env python3
"""Annotates a loomcc function's assembly with executions per frame.

    hotlines.py PROFILE.txt LISTING.lst SYMBOLS.sym LABEL [--min N]

PROFILE.txt is `loom-emulator trace --profile SYM --profile-focus LABEL`
output; LISTING.lst is loomcc's output assembled with build.py --listing.
Prints every line of LABEL's section with its executions a frame (lines
under --min are dimmed to a count of `.`), so hot paths read like the
side-by-side comparisons in docs/RESULTS.md.
"""
import re
import sys


def main(argv):
    prof, lst, sym, label = argv[1:5]
    mn = float(argv[argv.index("--min") + 1]) if "--min" in argv else 0.5
    start = None
    for line in open(sym):
        p = line.split()
        if len(p) == 2 and p[1] == label:
            start = int(p[0], 16)
    counts = {}
    on = False
    for line in open(prof):
        if line.startswith(f"addresses in {label}"):
            on = True
            continue
        if on:
            m = re.match(r"\s*([\d.]+)\s+([0-9a-f]{6})", line)
            if not m:
                if line.strip():
                    break
                continue
            counts[int(m.group(2), 16)] = float(m.group(1))
    lines = open(lst, errors="replace").read().splitlines()
    i = next(k for k, l in enumerate(lines) if l.strip() == f"{label}:")
    while i > 0 and ".SECTION" not in lines[i]:
        i -= 1
    total = 0.0
    for l in lines[i:]:
        if l.strip() == ".ENDS":
            break
        # Columns: line, ?, ?, ?, address in the bank, offset; bytes; text.
        m = re.match(r"\s*\d+ [0-9A-F]{4} [0-9A-F]{4} [0-9A-F]{4} ([0-9A-F]{4}) [0-9A-F]{4}\s+(?:[0-9A-F]{2} )+\s*(.*)$", l)
        if m:
            addr = (start & 0xFF0000) | int(m.group(1), 16)
            c = counts.get(addr, 0.0)
            total += c
            print(f"{c:8.1f}  {m.group(2)}" if c >= mn else f"{'.':>8}  {m.group(2)}")
        else:
            print(f"{'':8}  {l.strip()}")
    print(f"; {total:.1f} instructions a frame in {label}", file=sys.stderr)


if __name__ == "__main__":
    main(sys.argv)
