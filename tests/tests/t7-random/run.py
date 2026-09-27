#!/usr/bin/env python3
"""Differential testing with generated programs (T7).

    tests/t7-random/run.py --seeds 1-100 [--dir DIR] [--stmts N] [--narrow] [--shapes [--no-foreign] [--small]] [--recursion] [--loom] [--csmith] [runner options...]

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
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
def _llvm_bin():
    """$LLVM_BIN, else the directory of lli on PATH, else Homebrew's LLVM."""
    if os.environ.get("LLVM_BIN"):
        return Path(os.environ["LLVM_BIN"])
    lli = shutil.which("lli")
    return Path(lli).parent if lli else Path("/opt/homebrew/opt/llvm/bin")


LLVM = _llvm_bin()
HARNESS = ROOT / "harness/include"


CSMITH_DIR = HERE / "csmith"
CSMITH_FLAGS = ["--no-argc", "--no-longlong", "--no-math64", "--no-bitfields", "--no-packed-struct", "--no-float",
                "--max-funcs", "4", "--max-block-size", "3", "--quiet", "--concise"]


def host16_checksum(src, work, extra=(), init=None, more=()):
    """`more`: further C sources of the program (linked with llvm-link)."""
    lls = []
    for n, s in enumerate([src, *more]):
        ll = work / (src.stem + (f".m{n}" if n else "") + ".ll")
        r = subprocess.run(["taskpolicy", "-b", "nice", "-n", "19", str(LLVM / "clang"), "--target=msp430-none-elf", "-fsigned-char", "-std=c17", "-O0",
                            "-w", "-S", "-emit-llvm", "-DLOOMCC_T7_PRINT=1", f"-I{HARNESS}", *extra,
                            *([f"-ftrivial-auto-var-init={init}"] if init else []), str(s), "-o", str(ll)],
                           capture_output=True, text=True)
        if r.returncode:
            raise RuntimeError("clang: " + r.stderr[-500:])
        lls.append(ll)
    ll = lls[0]
    if len(lls) > 1:
        ll = work / (src.stem + ".linked.ll")
        r = subprocess.run([str(LLVM / "llvm-link"), "-S", "-o", str(ll), *map(str, lls)], capture_output=True, text=True)
        if r.returncode:
            raise RuntimeError("llvm-link: " + r.stderr[-500:])
    fix = ROOT / "harness/host16/fixup.py"
    pre = work / (src.stem + ".pre.ll")
    subprocess.run([sys.executable, str(fix), "pre", str(ll), str(pre)], check=True)
    opt = work / (src.stem + ".opt.ll")
    r = subprocess.run(["taskpolicy", "-b", "nice", "-n", "19", str(LLVM / "opt"), "-S", "-passes=instcombine<no-verify-fixpoint>", str(pre), "-o", str(opt)],
                       capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError("opt: " + r.stderr[-500:])
    subprocess.run([sys.executable, str(fix), "post", str(opt), str(opt)], check=True)
    r = subprocess.run(["taskpolicy", "-b", "nice", "-n", "19", str(LLVM / "lli"), "-force-interpreter", str(opt)], capture_output=True, text=True, timeout=120)
    if r.returncode:
        raise RuntimeError(f"lli exit {r.returncode}: " + r.stderr[-300:])
    last = r.stdout.split()[-1]
    return int(last, 16) if any("csmith" in e for e in extra) else int(last)


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
    shapes = "--shapes" in args
    if shapes:
        args.remove("--shapes")
    no_foreign = "--no-foreign" in args  # with --shapes: no calls into 816-tcc code
    if no_foreign:
        args.remove("--no-foreign")
    small = "--small" in args  # with --shapes: structs under ~110 bytes
    if small:
        args.remove("--small")
    loom = "--loom" in args  # Loom-shaped pools, ROM tables and hooks (gen.py)
    if loom:
        args.remove("--loom")
    widen = "--widen" in args  # a second module, an NMI handler, assembly calling back (gen.py)
    if widen:
        args.remove("--widen")
    recursion = "--recursion" in args  # recursive functions (gen.py)
    if recursion:
        args.remove("--recursion")
    use_csmith = "--csmith" in args
    if use_csmith:
        args.remove("--csmith")
    out = Path(take("--dir") or tempfile.mkdtemp(prefix="loomcc-t7-")).resolve()
    out.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="loomcc-t7-work-"))
    made = []
    for s in seeds:
        aux = {}
        if use_csmith:
            tmp = work / f"csmith-{s}.c"
            # Csmith writes platform.info into its working directory: keep it
            # in the scratch directory.
            subprocess.run(["taskpolicy", "-b", "nice", "-n", "19", "csmith", "--seed", str(s), *CSMITH_FLAGS, "-o", str(tmp)], check=True,
                           capture_output=True, cwd=work)
            prog = tmp.read_text()
            extra = [f"-I{CSMITH_DIR}"]
            # host16 widens pointers to 64 bits for lli, which changes the
            # size of a union with a pointer member, while clang types an
            # initialised array of such unions by its first member: indexing
            # then reads past the elements. host16 cannot model that program.
            if re.search(r"union U\d+ \{[^}]*\*", prog):
                print(f"seed {s}: a union with a pointer member (host16 cannot model it); skipped")
                continue
        else:
            prog = subprocess.run([sys.executable, str(HERE / "gen.py"), str(s), "--stmts", stmts] + (["--narrow"] if narrow else []) + (["--shapes"] if shapes else []) + (["--small"] if small else []) + (["--recursion"] if recursion else []) + (["--loom"] if loom else []) + (["--widen"] if widen else []),
                                  capture_output=True, text=True, check=True).stdout
            # A bundle: the main file, then `//@@FILE <suffix>` parts (a second
            # module, hand assembly), written beside the test.
            parts = prog.split("\n//@@FILE ")
            prog = parts[0]
            aux = {}
            for part in parts[1:]:
                suffix, _, body = part.partition("\n")
                aux[suffix.strip()] = body
            tmp = work / f"seed-{s}.c"
            tmp.write_text(prog)
            for suffix, body in aux.items():
                (work / f"seed-{s}.{suffix}").write_text(body)
            extra = []
        more = [] if use_csmith else [work / f"seed-{s}.{sfx}" for sfx in aux if sfx.endswith(".c")]
        try:
            ck = host16_checksum(tmp, work, extra, more=more)
            # A program whose result depends on memory lli does not model
            # (uninitialised storage, pointers host16 widens) gives a
            # different checksum from run to run: not a reference.
            if use_csmith and host16_checksum(tmp, work, extra, "pattern") != ck:
                print(f"seed {s}: host16 is not deterministic on it (reads memory it does not model); skipped")
                continue
        except Exception as e:  # a generator bug or a program host16 rejects: report, skip
            print(f"seed {s}: host16 failed ({str(e)[:200]}); skipped")
            continue
        if use_csmith:
            test = out / f"csmith-{s}.c"
            test.write_text("// loomcc-do: run\n// loomcc-int: 16\n"
                            f"// loomcc-options: -I%ROOT%/tests/t7-random/csmith -DEXPECTED=0x{ck:04x}u\n"
                            f"// loomcc-note: csmith --seed {s} {' '.join(CSMITH_FLAGS)}; checksum from host16\n"
                            "// loomcc-ref: host16\n// loomcc-max-frames: 3600\n// loomcc-timeout: 180\n" + prog)
        elif shapes:
            # 816-tcc cannot build frames over 255 bytes, so host16 is the
            # only reference.
            test = out / f"shapes-{s}.c"
            test.write_text("// loomcc-do: run\n// loomcc-int: 16\n"
                            f"// loomcc-options: -DEXPECTED={ck}u{' -DT7_NO_FOREIGN' if no_foreign else ''}\n"
                            f"// loomcc-note: generated by tests/t7-random/gen.py {s} --stmts {stmts} --shapes{' --small' if small else ''}; checksum from host16\n"
                            "// loomcc-ref: host16\n// loomcc-max-frames: 3600\n// loomcc-timeout: 180\n" + prog)
        else:
            test = out / f"seed-{s}.c"
            dirs = ""
            for suffix, body in aux.items():
                (out / f"seed-{s}.{suffix}").write_text(body)
                dirs += f"// loomcc-{'asm' if suffix.endswith('.asm') else 'extra'}-sources: seed-{s}.{suffix}\n"
            test.write_text("// loomcc-do: run\n// loomcc-int: 16\n" + dirs +
                            f"// loomcc-options: -DEXPECTED={ck}u\n"
                            f"// loomcc-note: generated by tests/t7-random/gen.py {s} --stmts {stmts}{' --narrow' if narrow else ''}; checksum from host16\n"
                            f"// loomcc-ref: {'host16' if widen else 'tcc-rom'}\n" + prog)
        made.append(test)
    print(f"{len(made)} programs in {out}")
    js = work / "results.json"
    # 816-tcc times out on --widen programs (and cannot build --shapes).
    refs = "host16" if use_csmith or shapes or widen else "tcc-rom"
    subprocess.run([str(ROOT / "run-tests"), "--refs", refs, "--json", str(js)] + args + [str(out)])
    res = {}
    for line in js.read_text().splitlines():
        r = json.loads(line)
        res.setdefault(r["test"], {})[r["mode"]] = r
    suspects = 0
    for t, m in sorted(res.items()):
        st = {k: m.get(k, {}).get("status", "-") for k in ("ir", "rom", "ref:tcc-rom")}
        known = [k for k in ("ir", "rom") if st[k] == "FAIL" and "No room for section" in m[k]["detail"]]
        if known:
            print(f"known F28/F30 (code too big for a bank): {t} [{', '.join(known)}]")
        real = [k for k in ("ir", "rom") if st[k] == "FAIL" and "not supported" not in m[k]["detail"] and k not in known]
        if real:
            suspects += 1
            other = m.get("ref:tcc-rom", m.get("ref:host16", {})).get("status", "-")
            print(f"LIKELY LOOMCC BUG: {t} [{', '.join(real)}] (reference: {other}): "
                  + "; ".join(m[k]["detail"].splitlines()[0] for k in real))
    print(f"{suspects} likely loomcc bugs")
    return 1 if suspects else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
