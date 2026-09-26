#!/usr/bin/env python3
"""Authoring aid: split a bundle into test files.

A bundle is text with `=== <path relative to tests/>` lines; each starts a
file that runs to the next `===` line. A `=== <path> !nonl` header writes
the file without a trailing newline. Existing files are overwritten.
"""
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent / "tests"
cur, nonl, buf = None, False, []

def flush():
    if cur is None:
        return
    p = root / cur
    p.parent.mkdir(parents=True, exist_ok=True)
    text = "\n".join(buf).strip("\n")
    p.write_text(text if nonl else text + "\n")

for line in open(sys.argv[1], encoding="utf-8").read().split("\n"):
    if line.startswith("=== "):
        flush()
        parts = line[4:].split()
        cur, nonl, buf = parts[0], "!nonl" in parts[1:], []
    else:
        buf.append(line)
flush()
