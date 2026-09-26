#!/usr/bin/env python3
"""Prints what a test prints when built by 816-tcc (or loomcc) and run on
the SNES harness: the authoring aid behind the T6 stage-2 expected outputs.

    scripts/rom-output.py tests/t6-loom/run/game-rng.c [--mode tcc-rom|rom] [-o FILE]

It runs the test through the runner with LOOMCC_TESTS_DUMP_OUTPUT set, which
makes the ROM harness read the WRAM output buffer back after the run.
"""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main(argv):
    if len(argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    test = Path(argv[1]).resolve()
    mode = argv[argv.index("--mode") + 1] if "--mode" in argv else "tcc-rom"
    out = argv[argv.index("-o") + 1] if "-o" in argv else None
    work = Path(tempfile.mkdtemp(prefix="rom-output-"))
    args = [str(ROOT / "run-tests"), "--keep", "--work", str(work), "-v"]
    args += ["--refs", mode, "--refs-only"] if mode.startswith("ref:") or mode == "tcc-rom" else ["--modes", mode]
    env = dict(os.environ, LOOMCC_TESTS_DUMP_OUTPUT="1")
    r = subprocess.run(args + [str(test)], env=env, capture_output=True, text=True)
    if "did not finish" in r.stdout or "loom-emulator failed" in r.stdout:
        print(r.stdout, file=sys.stderr)
        print("the ROM did not finish: raise loomcc-max-frames; no output captured", file=sys.stderr)
        return 1
    dumps = list(work.rglob("output.bin"))
    data = dumps[0].read_bytes() if dumps else b""
    if len(data) >= 4096:
        print("the output filled the 4 KiB harness buffer: print less", file=sys.stderr)
        return 1
    if not dumps:
        print(r.stdout + r.stderr, file=sys.stderr)
        print("no output captured", file=sys.stderr)
        return 1
    if out:
        Path(out).write_bytes(data)
    else:
        sys.stdout.buffer.write(data)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
