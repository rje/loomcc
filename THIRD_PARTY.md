# Third-party and non-MIT material

loomcc's own code is MIT licensed (see [LICENSE](LICENSE)). The repository
also contains test inputs from other projects. They are used only as test
and benchmark inputs; nothing from them is compiled into loomcc. Each keeps
its licence file beside it, and those files must stay with them.

## Vendored (copied into this repository)

| Material | Where | Origin | Licence | Licence file |
|---|---|---|---|---|
| mcpp 2.7.2 validation suite (cpp-test), n_* and e_* tests, text unchanged, loomcc directives appended | `tests/tests/t1-pp/mcpp/` | mcpp by Kiyoshi Matsui | BSD-2-Clause | `tests/tests/t1-pp/mcpp/LICENSE` |
| c-testsuite `tests/single-exec`: the 151 tests that are not from TinyCC, unmodified | `testbed/conformance/c-testsuite/` | c-testsuite `5c727565` | runners MIT; 150 tests from scc under ISC; `00001.c` MIT | `LICENSE`, `LICENSE.scc-ISC`, `single-exec/LICENSE` in that directory |
| chibicc `test/` (41 tests, `test.h`, `common`, `include/`), unmodified | `testbed/conformance/chibicc/` | chibicc `90d1f7f1` by Rui Ueyama | MIT | `testbed/conformance/chibicc/LICENSE` |
| Csmith 2.3.0 `safe_math.h`, reduced to 16-bit wrappers (`safe_math_16.h`) | `tests/tests/t7-random/csmith/` | Csmith, University of Utah | BSD-style | `tests/tests/t7-random/csmith/LICENSE.csmith` |
| PVSnesLib 4.6.0 example programs (9, with the headers they include) | `testbed/pvsneslib/examples/` | PVSnesLib (alekmaul) | zlib | `testbed/pvsneslib/LICENSE.pvsneslib-zlib.txt` |

Per-test origins for c-testsuite and chibicc are in
`testbed/conformance/manifest.json` and `testbed/conformance/README.md`;
the mcpp import is described in `tests/docs/SOURCES.md`.

## Fetched, never vendored

No GPL or LGPL code is in this repository. Copyleft suites, and large
ones, are downloaded at the pinned revisions instead.
`tests/external/fetch.sh <suite>` downloads them into
`tests/external/fetched/` (ignored by git); the repository holds only the
scripts, selection lists and wrappers that refer to them by path.

| Suite | Licence |
|---|---|
| GCC `gcc.c-torture/execute`, `gcc.c-torture/compile`, `gcc.dg/cpp`, `gcc.dg` | GPL-3.0-or-later |
| TinyCC `tests/tests2` | LGPL-2.1 |
| c-testsuite's 69 tests from TinyCC (`single-exec`, `.otags` naming bellard.org/tcc), used by `testbed/conformance` and the wrapped c-testsuite tier | LGPL-2.1 |
| LLVM test-suite `SingleSource/UnitTests`, `SingleSource/Regression/C` | Apache-2.0 WITH LLVM-exception (older files NCSA) |
| c-testsuite and chibicc as a whole (the copies the wrapped tiers run) | as above |

`fetch.sh` checks out the revisions recorded in `tests/docs/SOURCES.md`.
Without them the wrapped tiers report UNSUPPORTED. Tests in `tests/tests/`
that were found through a fetched suite are new, minimal programs written
for loomcc; their comments name the original test, but they do not copy it.

## Loom

`testbed/loom/`, the benchmark units and hand-written assembly pairs in
`testbed/bench/`, the inputs in `testbed/loom-build/` and
`tests/tests/t6-loom/` are C and 65816 assembly copied or derived from Loom,
the SNES game-making tool loomcc was written for, at the commits their
READMEs and PROVENANCE files name (for example `5599b9c` for
`testbed/loom/`, `d88b68b` for `tests/tests/t6-loom/`). Loom is MIT
licensed, Copyright (c) 2026 Ryan Evans.
