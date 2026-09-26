# Conformance tests

Third-party C conformance tests, copied unmodified, each with its licence.
Copyleft tests are not copied: c-testsuite's 69 tests from TinyCC (LGPL-2.1)
are fetched instead (see below).
`manifest.json` (written by `testbed/scripts/classify-conformance.py`) lists
every test with its origin, licence and heuristic flags.

## c-testsuite/single-exec (151 vendored, 69 fetched)

- Source: https://github.com/c-testsuite/c-testsuite, commit
  `5c7275656d751de0e68b2d340a95b5681858ed07` (2020-03-09), `tests/single-exec/`.
- Files per test: `NNNNN.c`, `NNNNN.c.expected` (the exact stdout+stderr; empty
  for 154 tests), `NNNNN.c.tags` (C standard, `needs-libc`, `needs-cpp`,
  `portable`), `NNNNN.c.otags` (the test's origin). A test passes when it
  exits 0 and its output equals `.expected`.
- Licences. The runners are MIT (`c-testsuite/LICENSE`). The tests carry
  their origin's licence (`single-exec/LICENSE` says so; `.otags` names it):
  - 150 tests from scc (git://git.simple-cc.org/scc,
    `tests/scc/execute/*`, scc version `355356a9…`): **ISC**,
    `LICENSE.scc-ISC` (copied from scc commit `66f99bce…`).
  - 69 tests from TinyCC (git://repo.or.cz/tinycc.git, `tests/tests2/*`,
    version `61ba9f22…`): **LGPL-2.1**, so **not in this repository**.
    `tests/external/fetch.sh c-testsuite` checks out c-testsuite at the
    commit above into `tests/external/fetched/c-testsuite/` (git-ignored);
    they are `tests/single-exec/NNNNN.c` there, and `manifest.json` lists
    them with `"vendored": false` and a `fetched:` path. Nothing from them
    goes into the compiler.
  - `00001.c` (no `.otags`; `return 0;`): c-testsuite's own, MIT.

## chibicc/test (41 tests)

- Source: https://github.com/rui314/chibicc, commit
  `90d1f7f199cc55b13c7fdb5839d1409806633fdb` (2020-12-07): `test/*.c`,
  `test/test.h`, `test/include{1..4}.h`, `test/common` (the `assert`, `printf`
  and ABI helpers linked into every test, built by the host C compiler), and
  `include/` (chibicc's own `stdarg.h`, `stddef.h`, …; the tests compile with
  `-Iinclude -Itest`).
- Licence: **MIT**, `chibicc/LICENSE`.
- Not copied: `test/driver.sh` (tests chibicc's command line) and
  `test/thirdparty/` (scripts that build sqlite, libpng and others).
- Every test uses `ASSERT(expected, expr)` → `assert()` → `printf`, and most
  use GNU statement expressions `({ ... })`. They assume LP64: `int` 4 bytes,
  `long` 8, pointers 8, and many `ASSERT(8, sizeof(...))` checks. They are
  front-end tests for loomcc (parse, type-check, constant evaluation) far more
  than execution tests; running them needs a `printf`, the `common` helpers
  compiled by loomcc and a rewrite of the size expectations.

## Relevance to a 16-bit-int compiler

Flag counts from `manifest.json` (grep heuristics over comment-stripped
source; a test can have several):

| flag | c-testsuite | chibicc |
|---|---|---|
| `float` (float/double types or literals) | 9 | 14 |
| `long-long` | 13 | 4 |
| `long` (no `long long`) | 8 | 12 |
| `varargs` (`...`, `va_*`) | 15 | 7 |
| `libc` (calls or `#include <…>`) | 71 | 41 |
| `printf` | 68 | 41 |
| `int-literal-over-16-bit` | 6 | 10 |
| `sizeof-of-int-long-or-pointer` | 6 | 1 |
| `gnu-or-c11-extension` (`__attribute__`, `typeof`, `({})`, `_Generic`, asm…) | 4 | 31 |
| `recursion` (a function that calls itself) | 5 | 1 |
| none of the above portability flags | **131** | 0 |

Read them as:

- **Likely irrelevant or out of scope** for the backend: `float`, `long-long`
  (loomcc diagnoses both), `varargs` (Loom uses none), and anything that needs
  `printf` or other libc unless the harness provides it.
- **Needs triage, not skipping**: `long` (loomcc's `long` is 32-bit, so these
  should work, but 816-tcc's 16-bit `long` will disagree), literals over 16
  bits and `sizeof` checks (results depend on `int` width; several c-testsuite
  tests silently assume 32-bit `int`, which no grep finds — expect some of
  the 131 to fail on a correct 16-bit-int compiler and check each by hand),
  and `recursion` (fine for the front end and the IR interpreter; the static
  frame allocator must handle it).
- The 131 unflagged c-testsuite tests are the first execution target: `int
  main()` returning 0, no libc, output compared with an empty `.expected`
  for most of them.

## Regenerating

```sh
git clone --depth 1 https://github.com/c-testsuite/c-testsuite
git clone --depth 1 https://github.com/rui314/chibicc
cp c-testsuite/tests/single-exec/* testbed/conformance/c-testsuite/single-exec/
# then delete the TinyCC-origin tests (their .otags name bellard.org/tcc):
grep -l bellard.org/tcc testbed/conformance/c-testsuite/single-exec/*.otags \
  | sed 's/\.otags$//' | while read t; do rm "$t" "$t.expected" "$t.tags" "$t.otags"; done
tests/external/fetch.sh c-testsuite      # so the manifest covers them
cp chibicc/test/*.c chibicc/test/*.h chibicc/test/common testbed/conformance/chibicc/test/
cp -R chibicc/include/. testbed/conformance/chibicc/include/
python3 testbed/scripts/classify-conformance.py
```

(Check the commit hashes above first; both upstreams move slowly.)
