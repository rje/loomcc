# loomcc-tests

The torture-test suite for [loomcc](../README.md), the SNES (WDC 65816) C
compiler. It proves the compiler phase by phase: preprocessor, parser,
semantic analysis, IR interpreter, 65816 back end, interop with 816-tcc, and
Loom's own C. Tests are annotated C files, like GCC's `dg` directives or
LLVM lit's `RUN` lines, and one runner executes them against a loomcc binary
and, optionally, against reference tools that establish what the right
answer is.

- `docs/PLAN.md`: the ladder (tiers T1 to T7, their oracles and targets)
- `docs/DIVERGENCES.md`: where clang and 816-tcc disagree with the suite, and why
- `docs/FINDINGS.md`: loomcc bugs the suite has found
- `docs/SOURCES.md`: external suites considered, licences, what was reused
- `docs/TCC-BUGS.md`: 816-tcc miscompiles found on the way, with reproducers

## Quick start

The suite is `tests/` in the loomcc workspace; its runner (`tests/runner`,
crate `loomcc-tests`) is a workspace member. From the repository root:

```sh
CARGO_BUILD_JOBS=2 taskpolicy -b nice -n 19 cargo build      # loomcc and the runner
./target/debug/loomcc-tests                                  # every tier, against target/debug/loomcc
./target/debug/loomcc-tests --tier t1 -v                     # one tier, every result
./target/debug/loomcc-tests tests/tests/t1-pp/variadic       # a directory or file
./target/debug/loomcc-tests --refs-only --tier t1            # check the suite itself against clang/816-tcc
```

`tests/run-tests` builds loomcc and the runner and then runs the runner with the same arguments.

The runner runs every tool under `taskpolicy -b nice -n 19` (the background
QoS band: nice alone does not stop Loom's ROM suites starving), uses at most
two jobs (`-j 2`), and runs at most one loom-emulator process at a time. The
scripts under `scripts/`, `external/` and `tests/t7-random/` do the same.

## Command line (stable)

```
loomcc-tests [options] [test paths or directories...]

  --loomcc PATH        loomcc binary (default: $LOOMCC, else the workspace's target/debug/loomcc,
                       else target/release/loomcc)
  --tier LIST          t1,t2,... or all (default all); prefixes match directory names
  --filter TEXT        only tests whose path contains TEXT
  --refs [LIST]        also run the reference tools: all that apply, or a list
                       (clang, clang16, tcc, host, host16, tcc-rom)
  --refs-only          run only the reference tools (validates the tests)
  --modes LIST         loomcc modes to run: E,syntax,S,ir,rom (default: all that apply)
  --xfail-list FILE    extra expected failures, one per line: `path [mode] # reason`
                       (mode: a loomcc mode, or ref:<tool> for a reference)
                       (a path may be a directory prefix)
  -j N                 parallel jobs (default 2)
  -v / -vv             print every result / also command lines and tool output on failure
  --json FILE          one JSON object per result
  --work DIR, --keep   scratch directory, and keep it afterwards
  --list               print the (test, mode) pairs and exit
  --pvsneslib DIR, --emulator PATH, --llvm-bin DIR, --clang PATH   tool locations
```

Environment: `LOOMCC`, `PVSNESLIB_HOME` (default `~/Library/Loom/Toolchains/v0/artifacts/pvsneslib`),
`LOOM_EMULATOR` (default `$LOOM_REPO/target/debug/loom-emulator`, with `LOOM_REPO`
defaulting to a checkout named `loom` beside this repository), `LLVM_BIN`.
Tests whose tools are missing report UNSUPPORTED.

Exit status: 0 when no result is FAIL, XPASS or UNRESOLVED, 1 otherwise, 2 for
a usage error.

### Results

Every (test, mode) pair gives one result:

| status | meaning |
|---|---|
| PASS | the expectation held |
| FAIL | it did not (wrong output, missing or unexpected diagnostic, crash, timeout, abort) |
| XFAIL | it failed and the test (or `--xfail-list`) says it is expected to |
| XPASS | it passed but was expected to fail: remove the xfail |
| UNSUPPORTED | the tool or loomcc mode is not there yet (see probes) |
| UNRESOLVED | the test itself is malformed, or the harness broke |

Failures print as `FAIL: <test> [<mode>]: <detail>`. The run ends with a
table per tier (reference results in their own `<tier> refs` rows) and the
summary line `loomcc-tests: N results: ...`.

### Before loomcc supports a tier

At start the runner probes each loomcc mode it needs with a trivial program
(`int main(void) { return 0; }`). A mode that does not succeed makes every
test of that mode UNSUPPORTED instead of FAIL, so running the whole suite
against an early loomcc is always meaningful. The probe line is printed:

```
loomcc modes: E yes, S no, ir no, syntax no
```

## How loomcc is driven

| mode | command (cwd = the test's directory) | pass when |
|---|---|---|
| `E` | `loomcc [options] -E test.c` | stdout tokens equal the expected tokens; diagnostics match |
| `syntax` | `loomcc -I<harness/include> [options] -fsyntax-only test.c` | diagnostics match; exit status non-zero iff an error is expected |
| `S` | `loomcc -I<harness/include> [options] -S test.c -o out.asm`, then `wla-65816` on it | exit 0 and the output assembles |
| `ir` | `loomcc -I<harness/include> [options] --run-ir test.c [extra sources]` | exit status 0 |
| `rom` | `loomcc -Dmain=loomcc_test_main -Dabort=loomcc_test_abort -Dexit=loomcc_test_exit -DLOOMCC_TEST_ROM=1 -I<harness/include> [options] -S test.c -o u0.asm`, assembled and linked with `harness/rom/harness.asm` and PVSnesLib's crt0/libc, run in loom-emulator | main returned 0 (or exit(0)) |

`--run-ir` is the contract this suite proposes for loomcc's IR interpreter:
compile the given sources, run `main` in the interpreter, and exit with main's
return value (abort() exits non-zero). `-S` output must be a WLA-DX unit in
the style of 816-tcc's (starting `.include "hdr.asm"`; the runner writes
`hdr.asm` beside it) whose externally visible functions follow the 816-tcc
ABI, since the harness calls `loomcc_test_main` with `jsl` and reads the
result from `tcc__r0`.

Diagnostics must be printed as `file:line[:col]: error: message` (or
`warning:`) on stderr, which is what clang, gcc and 816-tcc print.

## Test format

A test is a `.c` file under `tests/<tier>/...` with a `loomcc-do` directive.
Other files (headers, `.expected` files, helper sources) are support files.
Directives live in comments so every compiler ignores them; each is
`// loomcc-<name>: value` (or the same inside `/* */`), anywhere on a line.

| directive | meaning |
|---|---|
| `loomcc-do: preprocess \| syntax \| compile \| run` | the action (required) |
| `loomcc-options: -DX=1 -Iinc` | extra arguments for loomcc and every reference tool (quotes allowed; `%PVSNESLIB%` and `%ROOT%` expand to the PVSnesLib root and this repository) |
| `loomcc-expect: <tokens>` | expected `-E` output; all expect lines are joined in order and compared token by token (whitespace and line breaks do not matter; `+ +` is not `++`). An empty `loomcc-expect:` means "no tokens". A sidecar `<test>.expected` file may be used instead |
| `loomcc-error: <regex>` | an error must be reported on this line; the message must match the case-insensitive regex (empty = any message) |
| `loomcc-warning: <regex>` | the same for a warning |
| `loomcc-diagnostic: <regex>` | an error or a warning; used for constraint violations that compilers legitimately report either way, and for undefined behaviour worth diagnosing |
| `loomcc-error@+N:` `@-N` `@N` `@file:N` `@*` | the diagnostic's line relative to the directive, absolute, in another file (path as the compiler prints it), or anywhere |
| `loomcc-no-warnings` | any unexpected warning is a failure (unexpected errors always are) |
| `loomcc-xfail: <reason>` | loomcc is expected to fail this test |
| `loomcc-ref: clang, tcc` | the reference tools that apply (default per action, below; empty = none) |
| `loomcc-ref-diverges: <tool> [code] <reason>` | that reference tool is known to disagree (XFAIL for it); codes are listed in docs/DIVERGENCES.md |
| `loomcc-expect-stdout: <text>` | run tests: one line of expected printed output (lines joined with newlines, each ending in one) |
| `loomcc-expect-output: <file>` | run tests: the expected output is this file's bytes |
| `loomcc-expect-match: <regex>` / `loomcc-expect-no-match: <regex>` | preprocess tests: the `-E` output, line markers removed, must (not) match (Rust regex syntax; GCC's dg-final scan-file patterns) |
| `loomcc-int: agnostic \| 16` | run tests: whether the result is the same with 32-bit int (required for `run`) |
| `loomcc-skip-mode: ir rom host ...` | modes or references that do not apply |
| `loomcc-extra-sources: b.c` | more C sources, compiled by the compiler under test and linked in |
| `loomcc-tcc-sources: c.c` | C sources always compiled by 816-tcc (interop tests) |
| `loomcc-asm-sources: d.asm` | hand-written WLA-DX units linked into ROM runs |
| `loomcc-timeout: <seconds>` / `loomcc-max-frames: <n>` | limits (defaults 30-60 s; 300 frames) |
| `loomcc-note: ...`, `loomcc-source: ...` | documentation only |

Checks, in order: a crash (signal, Rust panic exit 101) or timeout is always
a FAIL; each expected diagnostic must be matched by a distinct actual one at
its line; any other error is a failure unless it is on a line that already
has an expected diagnostic (a cascade); the exit status must be non-zero
exactly when errors are expected; then the output is compared.

Reference tools use looser matching: any diagnostic at the line satisfies an
expected error or warning, messages are not compared, and extra warnings are
ignored.

### Reference tools (oracles)

| name | what runs | used by |
|---|---|---|
| `clang` | host clang: `clang -E -P -std=c17 -pedantic` | preprocess (default) |
| `clang16` | `clang --target=msp430-none-elf -std=c17 -pedantic -fsyntax-only`: 16-bit int, 32-bit long, clang's semantic checks | syntax, compile (default) |
| `tcc` | 816-tcc: `-E` for preprocess (line markers stripped), `-c` for syntax/compile | preprocess, syntax, compile (default) |
| `host` | host clang `-O1 -fsanitize=undefined -fsanitize-trap=all`, run natively | run tests marked `loomcc-int: agnostic` |
| `host16` | LLVM clang `--target=msp430-none-elf -emit-llvm`, pointers widened in the data layout, run by `lli -force-interpreter` | run tests (16-bit int semantics on the host) |
| `tcc-rom` | 816-tcc + 816-opt + wla-65816 + wlalink + loom-emulator, the same harness ROM loomcc's `rom` mode uses | run tests |

Printing: harness ROMs link `harness/rom/stdio.c` (always compiled by
816-tcc): `printf` (`%d %i %u %x %X %o %c %s %p %%`, width, `-`/`0` flags,
`l` = 32 bits), `puts` and `putchar` append to a 4 KiB WRAM buffer, and after
main returns 0 the harness compares it with the expected output, reporting
the first differing byte. Host runs compare stdout. `ir` mode compares the
interpreter's stdout, so `--run-ir` must implement `printf`/`puts`/`putchar`
(it does). host16 cannot run printing tests (UNSUPPORTED).

`harness/include/loomcc-test.h` gives every tool `i8/u8/i16/u16/i32/u32`,
`STATIC_CHECK(e)` (a negative-array-size static assertion 816-tcc
understands) and `CHECK(e)` (abort unless `e`).

## Layout

```
runner/            the runner (Rust, one dependency: regex)
harness/include/   loomcc-test.h, passed as -I to every tool
harness/rom/       harness.asm: main for execute ROMs, abort/exit, WRAM result words
tests/t1-pp/       preprocessor
tests/t2-parse/    lexing and parsing
tests/t3-sema/     constraints, conversions at 16-bit int, layout
tests/t4-exec/     self-checking execute tests
tests/t5-snes/     65816-specific: modes, banks, 816-tcc interop
tests/t6-loom/     Loom's runtime and generated C
tests/t7-random/   randomised differential testing (scripts, seeds)
external/          fetch scripts and filter lists for external suites (fetched/ is ignored)
vendor/            permissively licensed tests copied in, with their licences
harness/libc/      minimal stdio.h/stdlib.h/string.h for wrapped external suites
scripts/           authoring aids: gen-families.py (generated test families),
                   rom-output.py (what a test prints on the SNES), tcc-layout.py,
                   import-mcpp.py, divergences.py, split-bundle.py
```

Generated tests (`gen-*` directories) come from `scripts/gen-families.py`;
edit the script, not the files. Every generated expectation is checked by
the reference tools like any hand-written one.

## For the loomcc agent

Run the suite against every build and keep the summary line in your notes.
Do not edit this repository: if you believe a test is wrong, note it (test,
reason) in your own repo's notes for the suite owner to review. Expected
failures specific to your current milestone go in an xfail list kept in your
repo and passed with `--xfail-list`, not in the tests. The command
line above, the directive syntax and the mode contracts are stable; any
change will be announced in this file.
