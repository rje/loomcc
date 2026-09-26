#!/usr/bin/env python3
"""Differential testing with generated programs (T7).

    tests/t7-random/run.py --seeds 1-100 [--dir DIR] [--stmts N] [--narrow] [--csmith] [runner options...]

--csmith generates with Csmith (brew install csmith) instead of gen.py, using
tests/t7-random/csmith/csmith.h (16-bit-int limits, Csmith's safe math, a
16-bit checksum); the reference is host16.

For each seed: gen.py writes a program; host16 (clang --target=msp430,
16-bit int, run by lli) computes its checksum; the program is written as a
runner test with `-DEXPECTED=<checksum>`; then the runner runs loomcc (`ir`,
`rom`) and 816-tcc's ROM (`--refs tcc-rom`) over the lot. A seed where
loomcc disagrees with host16 while 816-tcc agrees is reported as a likely
loomcc bug; the test file stays in DIR for reduction (see README).

Generated programs are written to DIR (default: a scratch directory; pass
--dir tests/t7-random/corpus to keep them as regression tests).
"""
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
LLVM = Path(os.environ.get("LLVM_BIN", "/opt/homebrew/opt/llvm/bin"))
HARNESS = ROOT / "harness/include"


CSMITH_DIR = HERE / "csmith"
CSMITH_FLAGS = ["--no-argc", "--no-longlong", "--no-math64", "--no-bitfields", "--no-packed-struct", "--no-float",
                "--max-funcs", "4", "--max-block-size", "3", "--quiet", "--concise"]


def host16_checksum(src, work, extra=()):
    ll = work / (src.stem + ".ll")
    r = subprocess.run(["taskpolicy", "-b", "nice", "-n", "19", str(LLVM / "clang"), "--target=msp430-none-elf", "-fsigned-char", "-std=c17", "-O0",
                        "-w", "-S", "-emit-llvm", "-DLOOMCC_T7_PRINT=1", f"-I{HARNESS}", *extra, str(src), "-o", str(ll)],
                       capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError("clang: " + r.stderr[-500:])
    ll.write_text(ll.read_text().replace("p:16:16", "p:64:64").replace(" optnone", ""))
    opt = work / (src.stem + ".opt.ll")
    r = subprocess.run(["taskpolicy", "-b", "nice", "-n", "19", str(LLVM / "opt"), "-S", "-passes=instcombine<no-verify-fixpoint>", str(ll), "-o", str(opt)],
                       capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError("opt: " + r.stderr[-500:])
    opt.write_text(re.sub(r"= freeze (\S+) (.+)$", r"= bitcast \1 \2 to \1", opt.read_text(), flags=re.M))
    r = subprocess.run(["taskpolicy", "-b", "nice", "-n", "19", str(LLVM / "lli"), "-force-interpreter", str(opt)], capture_output=True, text=True, timeout=120)
    if r.returncode:
        raise RuntimeError(f"lli exit {r.returncode}: " + r.stderr[-300:])
    last = r.stdout.split()[-1]
    return int(last, 16) if extra else int(last)


def main(argv):
    args = argv[1:]
    def take(flag, default=None):
        if flag in args:
            i = args.index(flag)
            v = args[i + 1]
            del args[i:i + 2]
            return v
        return default
    seeds = take("--seeds", "1-20")
    lo, _, hi = seeds.partition("-")
    seeds = range(int(lo), int(hi or lo) + 1)
    stmts = take("--stmts", "12")
    narrow = "--narrow" in args
    if narrow:
        args.remove("--narrow")
    use_csmith = "--csmith" in args
    if use_csmith:
        args.remove("--csmith")
    out = Path(take("--dir") or tempfile.mkdtemp(prefix="loomcc-t7-")).resolve()
    out.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="loomcc-t7-work-"))
    made = []
    for s in seeds:
        if use_csmith:
            tmp = work / f"csmith-{s}.c"
            subprocess.run(["taskpolicy", "-b", "nice", "-n", "19", "csmith", "--seed", str(s), *CSMITH_FLAGS, "-o", str(tmp)], check=True,
                           capture_output=True)
            prog = tmp.read_text()
            extra = [f"-I{CSMITH_DIR}"]
        else:
            prog = subprocess.run([sys.executable, str(HERE / "gen.py"), str(s), "--stmts", stmts] + (["--narrow"] if narrow else []),
                                  capture_output=True, text=True, check=True).stdout
            tmp = work / f"seed-{s}.c"
            tmp.write_text(prog)
            extra = []
        try:
            ck = host16_checksum(tmp, work, extra)
        except Exception as e:  # a generator bug or a program host16 rejects: report, skip
            print(f"seed {s}: host16 failed ({str(e)[:200]}); skipped")
            continue
        if use_csmith:
            test = out / f"csmith-{s}.c"
            test.write_text("// loomcc-do: run\n// loomcc-int: 16\n"
                            f"// loomcc-options: -I%ROOT%/tests/t7-random/csmith -DEXPECTED=0x{ck:04x}u\n"
                            f"// loomcc-note: csmith --seed {s} {' '.join(CSMITH_FLAGS)}; checksum from host16\n"
                            "// loomcc-ref: host16\n// loomcc-max-frames: 3600\n// loomcc-timeout: 180\n" + prog)
        else:
            test = out / f"seed-{s}.c"
            test.write_text("// loomcc-do: run\n// loomcc-int: 16\n"
                            f"// loomcc-options: -DEXPECTED={ck}u\n"
                            f"// loomcc-note: generated by tests/t7-random/gen.py {s} --stmts {stmts}{' --narrow' if narrow else ''}; checksum from host16\n"
                            "// loomcc-ref: tcc-rom\n" + prog)
        made.append(test)
    print(f"{len(made)} programs in {out}")
    js = work / "results.json"
    refs = "host16" if use_csmith else "tcc-rom"
    subprocess.run([str(ROOT / "run-tests"), "--refs", refs, "--json", str(js)] + args + [str(out)])
    res = {}
    for line in js.read_text().splitlines():
        r = json.loads(line)
        res.setdefault(r["test"], {})[r["mode"]] = r
    suspects = 0
    for t, m in sorted(res.items()):
        st = {k: m.get(k, {}).get("status", "-") for k in ("ir", "rom", "ref:tcc-rom")}
        real = [k for k in ("ir", "rom") if st[k] == "FAIL" and "not supported" not in m[k]["detail"]]
        if real:
            suspects += 1
            other = m.get("ref:tcc-rom", m.get("ref:host16", {})).get("status", "-")
            print(f"LIKELY LOOMCC BUG: {t} [{', '.join(real)}] (reference: {other}): "
                  + "; ".join(m[k]["detail"].splitlines()[0] for k in real))
    print(f"{suspects} likely loomcc bugs")
    return 1 if suspects else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
