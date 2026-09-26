# External test suites: evaluation, licences, decisions

Rule: permissively licensed tests may be vendored (copied into this
repository) with their licence file and attribution kept beside them.
Copyleft tests (GPL, LGPL) are never vendored: `external/fetch.sh <suite>`
downloads them into `external/fetched/` (git-ignored), and the repository
holds only scripts, filter lists and wrappers that refer to them by path.

`external/fetch.sh gcc-torture tcc-tests2 llvm-singlesource c-testsuite chibicc mcpp`
fetches everything (sparse, shallow clones; about 100 MB). Commits fetched
on 2026-09-25 are recorded at the end.

| suite | licence | decision | where |
|---|---|---|---|
| mcpp 2.7.2 validation suite (cpp-test) | BSD-2-Clause (Kiyoshi Matsui) | **vendored**: n_* and e_* converted to T1 tests | `tests/t1-pp/mcpp/` (+ LICENSE, README) by `scripts/import-mcpp.py` |
| GCC `gcc.c-torture/execute` | GPL-3.0-or-later | **fetched, filtered, wrapped**; never copied | `external/classify-gcc-torture.py`, `external/gcc-torture-execute.list` |
| GCC `gcc.c-torture/compile`, `gcc.dg/cpp` | GPL-3.0-or-later | fetched; not yet wired (see "Next") | `external/fetched/gcc` |
| c-testsuite (`tests/single-exec`) | harness MIT; individual tests carry their origins' licences (ISC from scc, LGPL-2.1 from tinycc, others) | fetched only (mixed per-test licences; loomcc's own testbed already carries a manifest of them) | `external/fetched/c-testsuite` |
| chibicc `test/` | MIT (Rui Ueyama) | evaluated, not imported: every file relies on `printf`, `sprintf`, `strcmp` via `test/common`, and many assert 32-bit `int`/`long`/pointer sizes | `external/fetched/chibicc` |
| TinyCC `tests/tests2` | LGPL-2.1 | fetched only: output-compared through `printf` (`.expect` files), needs a libc on the target | `external/fetched/tinycc` |
| LLVM test-suite `SingleSource/UnitTests`, `Regression/C` | Apache-2.0 WITH LLVM-exception (older files NCSA) | fetched only for now: output-compared through `printf`, much floating point; permissive, so selected files may be vendored later with LICENSE.TXT | `external/fetched/llvm-test-suite` |

## mcpp (T1, vendored)

mcpp's cpp-test is the GCC-dg form of mcpp's validation suite: n_*
(conforming behaviour, one topic per C90/C99 clause), e_* (errors a
conforming preprocessor must diagnose), i_* (implementation-defined),
u_* (undefined), warn_*. `scripts/import-mcpp.py` imports n_* and e_*
unchanged, appending loomcc directives at the end so line numbers stay
exactly as mcpp wrote them:

- n_*: expected output from `clang -E -P -std=c17`, accepted only if it
  matches every `dg-final` grep pattern mcpp gives (so the expectation is
  mcpp's, the full token stream clang's);
- e_*: each `dg-error` becomes `loomcc-error` at its line (`@*` where mcpp
  says line 0); gcc's message wording is not checked.

Imported: 66. Skipped (reported by the script): tests that include hosted
headers (limits.h, ctype.h, string.h), C90/C95-only expectations
(`__STDC_VERSION__ == 199409L`, #if overflow of `long`), the trigraph test
whose pattern the Tcl-to-Python conversion cannot express, UCN tests clang
rejects, and one e_* test whose dg-error form is not understood. i_* and
u_* are not imported: implementation-defined and undefined behaviour are
covered by the suite's own tests where loomcc makes a choice.

## GCC torture execute (T4-style, fetched)

`external/classify-gcc-torture.py` selects tests for loomcc in two passes:

1. Static: no floating point (types or constants), no 64-bit integers, no
   GNU extensions that change meaning (attributes other than
   noinline/noclone/noipa/unused/... are rejected; harmless ones are defined
   away with `-D__attribute__(x)=`), no statement expressions, nested
   functions, labels as values, case ranges, vector types, `typeof`, `asm`,
   no builtins other than `__builtin_abort/exit` (mapped to abort/exit), no
   library functions, no system headers other than `stdlib.h`, no dg
   directives that need options or targets, no compiler-predefined type
   macros; a short hand-checked exclusion list (VLAs, GNU flexible-member
   initialisation).
2. Dynamic: the test must pass under host16 (clang's msp430 front end with
   16-bit `int` and 32-bit `long`, run by `lli`), i.e. it holds at 16-bit
   int.

The result is `external/gcc-torture-execute.list` (names only) and
`external/gcc-torture-execute.rejected` (every rejected name with its
reason). Wrappers in `external/fetched/gcc-wrapped/` `#include` the originals
with `loomcc-do: run`, `loomcc-int: 16` and a higher frame cap; run them
with

```sh
./run-tests --refs tcc-rom external/fetched/gcc-wrapped
```

They report under the tier `ext-gcc-wrapped`.

## Filters for 16-bit int (why tests drop out)

The biggest groups rejected (see the .rejected file for the exact list):
64-bit integers (`long long`), attributes with semantic effect, builtins,
floating point, dg directives (`dg-require-effective-target int32plus` and
similar are exactly the "needs 32-bit int" marker), system headers
(`limits.h`, `stdarg.h`, `stdio.h`), and then, dynamically, tests that
silently assume 32-bit `int` (they fail under host16).

## Fetched revisions

| suite | revision |
|---|---|
| gcc (gcc-mirror/gcc) | f0d56aeb912dd5e5ec7f81eaab23a3715e0c50a8 |
| c-testsuite | 5c7275656d751de0e68b2d340a95b5681858ed07 |
| chibicc | 90d1f7f199cc55b13c7fdb5839d1409806633fdb |
| tinycc (repo.or.cz) | 3dc99dbc82f8e07308c5d398136803e62f9676df |
| llvm-test-suite | 4c0824abd6755705b37248be0c83dc614154adbe |
| mcpp | 2.7.2 release tarball (SourceForge) |

## Next

- `gcc.c-torture/compile` as T2/T3 compile-only tests (same filter, `loomcc-do: compile`).
- `gcc.dg/cpp` as T1 tests: dg-do preprocess with dg-error/dg-final
  (the same conversion as mcpp, but fetched-and-wrapped, not vendored).
- c-testsuite through the same host16 filter as a T4 source (wrapped, not vendored).
- A printf shim in the harness ROM (a buffer in WRAM the runner reads) would
  open tcc tests2, chibicc and LLVM SingleSource, which compare printed output.
