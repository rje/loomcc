# The ladder

The suite climbs with loomcc's milestones (loomcc docs/PLAN.md section 6).
Each tier names its test format, its oracle (how the expected result is
known independently of loomcc) and a count target. A tier is useful as soon
as loomcc's matching mode exists; until then its tests are UNSUPPORTED.

| tier | loomcc milestone | action / mode | oracle | target | now |
|---|---|---|---|---|---|
| T1 preprocessor | M1 | `preprocess` / `E` | the C standard's text and examples; clang -E -P and 816-tcc -E as cross-checks; mcpp's validation suite | 400 | 458 (392 own + 66 mcpp) |
| T2 lexing and parsing | M2 | `syntax` / `syntax` | clang --target=msp430 -fsyntax-only, 816-tcc -c | 300 | 79 (seeded) |
| T3 semantics | M3 | `syntax` / `syntax` | clang --target=msp430 (16-bit int) and 816-tcc -c; layout pinned to 816-tcc | 300 | 87 (seeded) |
| T4 execute | M4 (ir), M5 (rom) | `run` / `ir`, `rom` | host clang (width-agnostic tests), host16 (msp430 IR under lli), 816-tcc ROM in loom-emulator | 600 | 155 own + 450 GCC torture (fetched) |
| T5 SNES-specific | M5-M6 | `run` with `tcc-sources`/`asm-sources` | 816-tcc ROM, hand assembly | 100 | 9 (seeded) |
| T6 Loom-realistic | M8 | `compile`, then `run` | 816-tcc build of the same files; Loom ROM tests | Loom's whole runtime | 31 units (stage 1), 4 differential drivers (stage 2) |
| T7 randomised | M4 onwards | generated `run` tests | host16 checksum vs loomcc ir/rom vs 816-tcc ROM | continuous | generator + 40-program corpus |

Counts: `./run-tests --list | cut -d' ' -f1 | sort -u | wc -l`, or the
per-tier table of any run.

## Principles

- **One behaviour per test**, small enough that a failure explains itself.
  The name says what is tested; a `loomcc-note` says why when it is subtle
  (and cites the standard's section).
- **The oracle is never loomcc.** Every expected result is justified by the
  standard, cross-checked by at least one independent tool, or both. Where a
  reference tool disagrees with the standard the test says so with
  `loomcc-ref-diverges` and a code from docs/DIVERGENCES.md, and the runner
  keeps checking that it still disagrees (a declared divergence that starts
  agreeing is an XPASS).
- **16-bit int is the default world.** Tests that depend on int width say
  `loomcc-int: 16` and never run on a 32-bit-int host; width-agnostic ones
  say `agnostic` and run everywhere.
- **Implementation-defined choices are pinned to 816-tcc** where Loom's
  assembly or ROM tests can observe them (layout, `char` signedness, `__FILE__`
  spelling, bit-field allocation), and to the loomcc plan otherwise (`long` is
  32 bits). The tests say which.
- **Undefined behaviour is not tested for a particular result.** It appears
  only as `loomcc-diagnostic` (a diagnostic is good practice: invalid pastes,
  `#line 0`) or as "must not crash".
- **Diagnostics are required where the standard requires them**
  (constraint violations and syntax rules). `loomcc-error` is used when every
  sensible compiler makes it an error; `loomcc-diagnostic` when warning is
  equally conforming.

## T1: preprocessor (populated)

Format: `// loomcc-do: preprocess` with `loomcc-expect` token lines and
`loomcc-error/diagnostic` lines. Comparison is token by token after
re-lexing both sides, so line breaks and spacing are free but adjacent tokens
that would merge (`+ +` printed as `++`) are caught.

Oracle: the standard (C17 6.10, the examples of 6.10.3.5, 6.10.3.3, 6.10.2,
6.10.9, and C23 6.10.5.1 for `__VA_OPT__`); every expected output is also
checked against `clang -E -P -std=c17 -pedantic` and `816-tcc -E` by
`--refs`. 816-tcc's preprocessor is far from conforming; its divergences
are catalogued (docs/DIVERGENCES.md).

Directories:

| directory | covers |
|---|---|
| `lex` | phases 1-3: splices, comments, pp-numbers (`0xe+1`), digraphs, literal prefixes, max munch, `-E` spacing, UCNs, trigraphs (xfail by design) |
| `object` | object-like macros, redefinition rules (6.10.3p2), self-reference, `defined` as a name, expansion to function names |
| `function` | invocation syntax, arguments (empty, nested, newlines, strings), argument pre-expansion, parameter-list errors, arity errors |
| `rescan` | rescanning and painted-blue tokens (6.10.3.4): Prosser hide sets, the 6.10.3.4p4 example, deferred expansion and EVAL recursion (where a pure hide-set implementation over-paints) |
| `stringize` | `#`: spacing, escaping, empty arguments, constraints |
| `paste` | `##`: placemarkers, operators, prefixes, constraint errors, invalid pastes, the `hash_hash` example, Loom's `LOOM_STATIC_ASSERT` |
| `variadic` | `...`, `__VA_ARGS__`, `__VA_OPT__` (every C23 example), constraint errors |
| `cond` | `#if/#ifdef/#elif/#else/#endif`, skipped groups, `defined`, intmax_t/uintmax_t arithmetic (not 16-bit int), unsigned conversions, short circuits, errors |
| `include` | "" vs <> search order, includer-relative lookup, -I order, -iquote, guards, `#pragma once`, computed includes, `__has_include`, recursion limits, errors in headers |
| `line` | `__LINE__`, `__FILE__`, `#line` (decimal, max, with macros, moving diagnostics), errors |
| `predef` | `__STDC__`, `__STDC_VERSION__` (201710L), `__STDC_HOSTED__`, `__COUNTER__`, `__loomcc__`/`__65816__` |
| `pragma` | `#pragma` pass-through, `_Pragma` destringizing, errors |
| `directive` | null directive, `#error`, `#warning`, unknown directives, `#define`/`#undef` errors |
| `std` | the standard's examples, verbatim |
| `lex-errors` | unterminated literals and comments, UB that must not crash |

Target 400; at the first checkpoint 392 own tests plus 66 imported from mcpp.

Not tested on purpose: directives inside macro arguments (undefined),
`defined` produced by macro expansion (undefined), `#include_next` and other
GNU extensions, the GNU `, ## __VA_ARGS__` comma swallow (clang applies it
even in `-std=c17`; loomcc's plan does not list it), multi-line invocation
`__LINE__` (unspecified).

## T2: lexing and parsing (seeded)

Format: `// loomcc-do: syntax`; valid programs must be accepted with no
error; invalid ones carry `loomcc-error` at the offending line. Where a
parse is ambiguous in text (typedef names), the test forces a semantic
consequence (`STATIC_CHECK(sizeof(x) == 1)`) so that the wrong parse is
rejected.

Oracle: the C17 grammar (A.2); cross-checked with
`clang --target=msp430-none-elf -fsyntax-only -pedantic` and `816-tcc -c`.

Areas: declarators and abstract declarators (pointers to arrays of
functions, parenthesised declarators, `(*(*f)(int))[3]`), type names in
casts/sizeof/compound literals, typedef-name ambiguity and scoping
(redeclaring a typedef name as an object in an inner scope; `T(x);`),
declaration specifiers in any order, storage classes, qualifiers,
`_Alignas`, `_Static_assert`, `_Generic`, `_Noreturn`, bit-fields, enums
(trailing comma), designated initialisers (nested, array ranges no, repeated
designators), compound literals, every statement (labels, `switch` with
`case` in nested blocks, Duff's device, `goto`, empty statements),
every expression form and precedence level, `sizeof` of expressions vs types,
string literal concatenation, K&R definitions (rejected? loomcc: C17 still
allows them; tested as accepted), and syntax errors (missing `;`,
unbalanced braces, `else` without `if`, bad declarators) that must be
rejected at the right line.

Target 300.

## T3: semantics (seeded)

Format: `// loomcc-do: syntax`, with `STATIC_CHECK(expr)` (from
harness/include/loomcc-test.h; a negative array size is a constraint
violation every compiler rejects) for compile-time facts and `loomcc-error`
for required diagnostics.

Oracle: `clang --target=msp430-none-elf` is a conforming front end with
16-bit int and 32-bit long, so every promotion and conversion check is
confirmed by it; 816-tcc confirms layout (sizeof, offsetof, bit-field
allocation) because loomcc must match 816-tcc's layout rules (pointers are 4
bytes, 4-aligned inside structs; long differs: 816-tcc's long is 16 bits, so
long-sized checks use `ref-diverges: tcc`). Values derived from 816-tcc are
produced with `scripts/tcc-layout.sh`, which compiles a probe with 816-tcc
and prints the `.dw` values from its assembly output.

Areas:
- integer promotions at 16-bit int: `unsigned short` promotes to `unsigned
  int` (16 bits) not `int`; `unsigned char` to `int`; `u16 * u16` does not
  overflow into 32 bits; `-1 < 1u` is false; `65535u + 1u == 0u`; `32767 + 1`
  overflows (a constant expression that must be diagnosed);
- usual arithmetic conversions with `long` (32 bits): `-1L < 1u` is true
  (unsigned int converts to long), `-1 < 1ul` false;
- integer constant types at 16 bits: `32768` is `long`, `0x8000` is
  `unsigned int`, `65536` is `long`, `0xFFFFFFFF` is `unsigned long`;
- constraint violations (6.5.x Constraints): assigning incompatible
  pointers, `&` of a register variable or bit-field, modifying const,
  calling a non-function, wrong argument counts to prototypes, duplicate
  members, `break` outside a loop, duplicate `case`/`default`, redefinition
  of objects and labels, incomplete types, `void` objects, arrays of
  functions, `sizeof` a function or bit-field, `_Static_assert` failures;
- layout: every struct shape Loom uses, pointer members (align 4), bit-fields
  (816-tcc: LSB first, units of the declared type, no straddling),
  unions, nested and array members, tail padding;
- constant expressions: `sizeof`, casts, `?:`, enumerators, address
  constants in initialisers;
- `char` is signed; `'\xff' == -1`; plain bit-field signedness.

Target 300.

## T4: execute (seeded, growing)

Format: `// loomcc-do: run` programs in the style of
gcc.c-torture/execute: they return 0 or call `abort()`, and use no libc
beyond `abort`. Every test says `loomcc-int: agnostic` (the same result with
32-bit int: explicit fixed-width types, no reliance on promotion widths) or
`loomcc-int: 16`.

Oracles, all three run by `--refs`:
- `host`: native clang with trapping UBSan, for agnostic tests: proves the
  test is correct and free of undefined behaviour;
- `host16`: clang's msp430 front end (16-bit int, 32-bit long) to LLVM IR,
  run in `lli -force-interpreter` with pointers widened: the reference for
  16-bit-int semantics on the host;
- `tcc-rom`: the test compiled by 816-tcc, linked with the harness ROM and
  run in loom-emulator: the real machine and the incumbent compiler. 816-tcc
  bugs and its 16-bit long are declared with `ref-diverges: tcc-rom`.

loomcc runs each test twice: `ir` (the interpreter, M4) and `rom` (the
65816 back end in the emulator, M5).

Areas: arithmetic at 8/16/32 bits (wraparound for unsigned, conversions,
shifts by variable and constant amounts, division and modulo by constants,
powers of two and variables, signed/unsigned comparisons), `char` and
`signed char` sign extension, structs (copy, return, pass by value, nested,
arrays of structs, bit-fields), arrays (1-D, 2-D, indexing with every
integer type, negative indices through pointers), pointers (arithmetic,
comparison, difference, pointers to pointers, `const`), control flow
(`switch` dense and sparse, fall-through, `default` first, loops of every
form, `break`/`continue`, `goto`), recursion, function pointers (tables,
callbacks, returning them), initialisers (static and automatic, designated,
partial, strings into arrays), globals and statics (initialised RAM vs
BSS), volatile.

Target 600: grow by area, driven by what loomcc's backend does (every
addressing mode, every compare-and-branch idiom, 8-bit regions).

## T5: SNES-specific (seeded)

- 8-bit/16-bit mode transitions: functions that work on `u8` data between
  16-bit calls; results must survive `sep/rep` regions (checked through
  values, and with `trace --profile` for the M flag at call boundaries).
- Far data: `const` tables in ROM banks above bank 0 (`lda.l`), tables
  crossing a bank boundary (a 64 KiB+ array in a HiROM/LoROM layout),
  pointers whose bank byte matters, pointer arithmetic across a bank.
- Interop with 816-tcc (`loomcc-tcc-sources`): loomcc calls 816-tcc code and
  the reverse; argument slots of 1/2/4 bytes; struct by value both ways;
  struct returns through the hidden pointer; function pointers passed across
  (`tcc__jsl_r10`); globals defined by one compiler and used by the other;
  statics with the same name in both units.
- Hand assembly (`loomcc-asm-sources`) reading structs by offset, as Loom's
  body.asm and oam.asm do.
- Loom's static assertions: every `LOOM_STATIC_ASSERT` in Loom's headers
  compiled by loomcc must pass.
- Interrupt context: a function reached from NMI (`nmiSet`) and main-line
  code must not share static frames (a ROM test where both run).

Oracle: the same programs built entirely with 816-tcc (`tcc-rom`) and,
for asm interop, hand-computed expectations.

## T6: Loom-realistic (stages 1 and 2 running)

Loom's runtime C, generated C and hooks, copied from /Users/rje/src/rust/loom
at d88b68b (`tests/t6-loom/loom-d88b68b/PROVENANCE`).

- Stage 1 (`tests/t6-loom/compile`): `compile` every unit with Loom's include
  paths and debug definitions; the output must assemble. 31 units.
- Stage 2 (`tests/t6-loom/run`): differential runs. A driver `#include`s one
  runtime unit (so its static helpers are callable), calls its functions over
  grids of inputs and fixed call sequences, and prints the results through the
  harness printf. The expected output is what the same driver prints when
  816-tcc builds it (captured with `scripts/rom-output.py`), so loomcc must
  agree with Loom's current compiler on Loom's own code. Drivers cast `char`
  results to `unsigned` before printing (816-tcc bug 5 in docs/TCC-BUGS.md).
  Units so far: game.c (RNG, timers), adventure.c (flags, gates, actions,
  request queue), camera.c (facing sign, auto-scroll step, approach),
  animation.c (direction selection).
- Stage 3 belongs to loomcc's M8: a Loom sample ROM built with loomcc passing
  Loom's own ROM tests.

## T7: randomised differential testing (running)

`tests/t7-random/gen.py` generates UB-free self-checking programs in
loomcc's subset (8/16/32-bit integers, arrays, calls, loops, `if`,
`switch`; `--narrow` for 8/16-bit only); `tests/t7-random/run.py` computes
each program's checksum with host16, then runs loomcc (`ir`, `rom`) and the
816-tcc ROM, and reports LIKELY LOOMCC BUGs. See tests/t7-random/README.md,
including the Csmith/YARPGen configuration and the cvise interestingness test
for when those tools are installed (they are not on this machine yet). The
first campaign (60 `--narrow` seeds) found no loomcc disagreement; 40 of the
programs are kept in `tests/t7-random/corpus`.

## Maintenance

- A new test is checked with `--refs-only` before it is committed: every
  reference either agrees or has a declared, coded divergence.
- A loomcc failure that is a real bug goes to docs/FINDINGS.md with the
  test, the expected result and what loomcc did. The test is not changed.
- Reference-tool divergences go to docs/DIVERGENCES.md under their code.
