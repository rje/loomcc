# loomcc

loomcc is a C compiler for SNES games, written from scratch in Rust. It
targets the WDC 65816 and emits WLA-DX assembly that links beside objects
from 816-tcc (PVSnesLib's compiler), PVSnesLib's crt0 and libc, and
hand-written assembly. It was written to make the C in the SNES games made
with Loom (a game-making tool, not yet public) run faster than it does
through 816-tcc: Loom's runtime, its generated tables and the game's own
hooks.

It is a *whole-program* compiler: every C unit of a game goes in at once,
so it can inline across units, give each function a static frame in WRAM
instead of a stack frame, and keep values in direct page and X/Y. Functions
other code can call keep 816-tcc's calling convention, so either compiler
(or hand assembly) can call the other.

## Status

loomcc is young (written in September 2026) and built for one codebase.
It is not a general-purpose C compiler.

- It compiles all of Loom's C and builds a complete game ROM that behaves
  identically to the 816-tcc build (see Results).
- Deliberate differences from 816-tcc: `long` is 32 bits (816-tcc's is 16);
  internal calls use their own convention (only entry points follow
  816-tcc's). Struct layout matches 816-tcc.
- Not supported: floating point, `long long` in the back end (the front end
  and IR interpreter have it), variable-length arrays and most GNU
  extensions.
- Known open issues are listed in
  [tests/docs/FINDINGS.md](tests/docs/FINDINGS.md): some preprocessor corner
  cases (deferred rescans, `__VA_OPT__` details), constraint diagnostics
  from GCC's testsuite, UTF-8-only source files, functions larger than a
  ROM bank, and defining variadic functions.

## Results

From [docs/RESULTS.md](docs/RESULTS.md), measured in an emulator
(loom-emulator, MesenCore) over 31 benchmarks taken from Loom: each one runs
the same C under 816-tcc (with 816-opt), under loomcc, and, for 24 of them,
as the hand-written assembly that replaced that C in Loom.

| loomcc relative to | clocks | instructions | code + rodata bytes |
|---|---:|---:|---:|
| 816-tcc (31 benchmarks) | 0.38x | 0.35x | 0.50x |
| hand assembly (24 pairs) | 1.28x | 1.37x | 1.43x |

All 31 produce identical results in every variant.

Loom's Cliffside sample, built entirely with loomcc (27 C units, Loom's
hand assembly unchanged), runs 28% fewer instructions per game tick than
the 816-tcc build (7,682 against 10,622 in Loom's own verification;
RESULTS.md records 7,956 against 11,284, -29.5%, on an earlier snapshot)
and is identical on the tick clock: the 108-byte state witness agrees at all
813 ticks and the sequence of presented frames is the same. Its C code is
0.49x the size.

## Building

```sh
cargo build --release
```

This builds `target/release/loomcc` (the compiler), `loomcc-bench` (the
benchmark driver) and `loomcc-tests` (the test runner). There are no
dependencies beyond crates.io.

### Using the driver

```
loomcc [-I dir] [-iquote dir] [-isystem dir] [-D n[=v]] [-U n] [-nostdinc] [-O0|-O1|-O2] MODE inputs... [-o out]
  -E              preprocess (default mode)
  --tokens        one preprocessing token per line
  -fsyntax-only   parse and type-check
  --print-ast     parse and print the AST back as C
  --emit-ir       whole program (all inputs) to IR text
  --run-ir        whole program to IR, run main() in the IR interpreter
  -S              whole program to one WLA-DX .asm
  --asm-callbacks=FILE.asm   hand assembly whose jsl targets call back into C
```

All the C inputs of a program go on one command line; `-S` writes a single
`.asm` for `wla-65816`. loomcc ships freestanding headers (stddef, stdint,
stdbool, limits, stdarg, stdio, stdlib, string, assert) in
`crates/driver/include`, searched unless `-nostdinc`. Details and the
interop rules are in [docs/PLAN.md](docs/PLAN.md) §4, §5 and §5b.

### Environment

Only the tests and benchmarks need outside tools; the compiler needs none.

| variable | used for | default |
|---|---|---|
| `PVSNESLIB_HOME` | PVSnesLib and devkitsnes (816-tcc, wla-65816, wlalink, headers) | `~/Library/Loom/Toolchains/v0/artifacts/pvsneslib` |
| `LOOM_REPO` | a Loom checkout, for `loom-emulator` (benchmarks, ROM tests) | a sibling directory named `loom` |
| `LOOM_EMULATOR` | the emulator binary, overriding `LOOM_REPO` | `$LOOM_REPO/target/debug/loom-emulator` |
| `LOOMCC` | the loomcc binary the tests and benchmarks run | `target/debug/loomcc` |
| `LLVM_BIN` | LLVM tools (`lli`) for the 16-bit-int reference | found on `PATH` |

Loom is not public, so the emulator-based parts (ROM tests, benchmarks,
the Cliffside build) currently cannot be reproduced outside it.

## Tests

```sh
cargo test --release
```

runs the unit tests and the testbed front-end checks. Those that need
816-tcc, PVSnesLib's headers or clang skip when they are not installed.

The conformance and torture suite lives in [tests/](tests/README.md): about
1,850 annotated C files in seven tiers (preprocessor, parser, semantics, IR
execution, SNES ROM execution, Loom's own C, random programs), checked
against loomcc and, optionally, against clang, a 16-bit-`int` clang and
816-tcc as references.

```sh
cargo build                                    # loomcc and the runner
tests/run-tests --tier t1                      # one tier
tests/run-tests --tier t1,t2,t3,t4 --modes E,syntax,ir   # no toolchain needed
tests/run-tests                                # everything that applies
```

Tiers that build ROMs need `PVSNESLIB_HOME` and `loom-emulator`. No GPL
or LGPL code is in the repository. The external suites (GCC torture and dg
tests, TinyCC's tests2, c-testsuite including its TinyCC-derived tests,
LLVM's single-source tests) are downloaded at pinned revisions and wrapped:

```sh
tests/external/fetch.sh c-testsuite
tests/external/wrap-suite.py --from-list c-testsuite
tests/run-tests tests/external/fetched/wrapped-c-testsuite
```

Unfetched suites report UNSUPPORTED. See
[tests/docs/SOURCES.md](tests/docs/SOURCES.md) and
[THIRD_PARTY.md](THIRD_PARTY.md).

## Layout

```
crates/pp       loomcc-pp      lexer and C17 preprocessor
crates/parse    loomcc-parse   recursive-descent parser to an AST
crates/sema     loomcc-sema    types, 816-tcc-compatible layout, constant evaluation
crates/ir       loomcc-ir      IR (CFG over typed virtual registers), lowering, interpreter
crates/opt      loomcc-opt     optimiser, including whole-program passes (inlining)
crates/w65816   loomcc-w65816  65816 back end: instruction selection, allocation, WLA-DX
crates/driver   loomcc         the command-line driver and its headers
crates/bench    loomcc-bench   benchmark runner and RESULTS tables
testbed/        benchmarks (C, drivers, hand-asm pairs), Loom's C corpus, harness
tests/          the conformance and torture suite and its runner (tests/runner)
docs/           PLAN.md (design and log), RESULTS.md (measurements), results/
```

- [docs/PLAN.md](docs/PLAN.md): the design, 816-tcc's ABI, and the running log of decisions
- [docs/RESULTS.md](docs/RESULTS.md): benchmark and Cliffside measurements
- [testbed/README.md](testbed/README.md) and [testbed/bench/README.md](testbed/bench/README.md): the corpus and the benchmark harness
- [tests/README.md](tests/README.md), [tests/docs/](tests/docs/): the suite, its plan, findings and divergences

## Licence

MIT (see [LICENSE](LICENSE)). The repository also contains third-party test
code and Loom's C under their own terms; see [THIRD_PARTY.md](THIRD_PARTY.md).
