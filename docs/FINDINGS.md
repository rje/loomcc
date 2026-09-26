# Findings: loomcc behaviour the suite believes is wrong

Each entry: the test(s), the expected result (with the rule behind it), and
what loomcc did, with the loomcc commit it was seen at. Entries move to
"Fixed" with the commit that fixed them; tests are never edited to match
loomcc. The runner prints the build time of the loomcc binary it ran.

Latest full run: loomcc 179dd28 (build of 2026-09-26 12:10 UTC): 2436 results, 2410 PASS, 18 FAIL (all preprocessor and constraint diagnostics: F3, F4, F23, F27). F20 and F24 still reproduce through the wrapped external suites.

## Open

### F3. Pure hide sets over-paint in deferred expansion (since 2ee694c)

Tests: `t1-pp/rescan/defer-recursion.c`, `t1-pp/rescan/eval-repeat.c`.

```c
#define EMPTY()
#define DEFER(id) id EMPTY()
#define EXPAND(...) __VA_ARGS__
#define A() 1 DEFER(B)()
#define B() 2 DEFER(A)()
EXPAND(EXPAND(A()))
```

Expected `1 2 1 B ()`; loomcc prints `1 2 A ()`. C17 6.10.3.4p2 paints a
name only when it is met while *that* macro's replacement is being
rescanned. A's replacement is finished when B's expansion produces the
second A, so that A stays available; gcc, clang and MSVC's conforming
preprocessor agree. In Prosser's algorithm the hide set of B's invocation is
`HS(B) ∩ HS(")") ∪ {B}`, and both `B` and its `)` came from A's
replacement, so A is in it and the new A is painted. The EVAL/DEFER
recursion idiom (Boost.PP, P99, Cloak) depends on the standard behaviour.
Loom's C does not use it: low priority.

### F4. `__VA_OPT__` details (since 2ee694c; the diagnostics were fixed in 1fc1258)

- `t1-pp/variadic/va-opt-c23-examples.c`, `va-opt-empty-expansion.c`: `F(EMP)`
  with `#define EMP` must give `f(0)` (C23 6.10.5.1 EXAMPLE: `__VA_OPT__`
  tests whether the variable arguments *expand* to nothing); loomcc gives
  `f(0,)`.
- `va-opt-paste.c`: `#define H4(X, ...) __VA_OPT__(a X ## X) ## b` with
  `H4(, 1)` must give `a b` (the `__VA_OPT__` result ends in a placemarker,
  and that is what `## b` pastes with); loomcc gives `ab`.
- `va-opt-stringize.c`: `#__VA_OPT__(...)` must stringize the replacement
  (`H3(, 0)` gives `""`); loomcc leaves `#` in the output.

### F23. Preprocessor diagnostics still missing (1fc1258)

| test | rule | loomcc |
|---|---|---|
| `t1-pp/variadic/too-few-variadic-args.c` (`M(x)` for `M(X, ...)`) | C17 6.10.3p4 (constraint; C23 relaxes it) | silent |
| `t1-pp/predef/undef-predefined.c` (`#undef __FILE__`) | 6.10.8p2 (undefined; gcc and clang warn) | silent for `__FILE__` (`__STDC__` is diagnosed) |
| `t1-pp/mcpp/e_ucn.c` line 6 (`#define macro\U0000001F`) | 6.4.3p2: a UCN may not name a control character | silent |

The wrapped GCC `gcc.dg/cpp` tests also flag `defined` produced by macro
expansion (`defined.c`) and a directive inside macro arguments
(`mac-dir-2.c`); both are undefined behaviour that gcc diagnoses, so they are
quality issues only.

### F27. Constraint violations found by the new T2/T3 tests (cac2523)

| test | rule | loomcc |
|---|---|---|
| `t2-parse/errors/array-of-void.c` (`void a[3];`) | 6.7.6.2p1 | accepted |
| `t2-parse/errors/auto-at-file-scope.c`, `register-at-file-scope.c` | 6.9p2 | accepted |
| `t2-parse/errors/empty-struct.c` (`struct Empty { };`) | 6.7.2.1p8 (syntax: a member is required) | accepted |
| `t2-parse/errors/enum-member-repeated.c` (`enum E { A, B, A };`) | 6.7.1 (redeclaration) | accepted |
| `t2-parse/errors/restrict-on-non-pointer.c` (`restrict int x;`) | 6.7.3p2 | accepted |
| `t3-sema/constraint/redeclare-different-linkage.c` (`int x; static int x;`) | 6.2.2p7 (undefined; clang errors) | silent |
| `t3-sema/constraint/sizeof-void.c` | 6.5.3.4p1 | silent |
| `t3-sema/expr/pointer-compare-mismatch.c` (`int *` == `char *`) | 6.5.9p2 | silent |

### F28. A function bigger than a ROM bank cannot link (cac2523, rom; low priority)

Csmith seed 239 (`tests/t7-random/run.py --csmith --seeds 239`): loomcc
compiles `func_1` to 34,570 bytes, and wlalink reports `No room for section
"lcc.lcsu0_0_func_1" (34570 bytes) in ROM bank 0`. A 32 KiB LoROM bank is
the hard limit for one SUPERFREE section; loomcc could split huge functions,
limit inlining into them, or at least say which function is too big.
816-tcc cannot assemble this program either (its stack offsets overflow).

### F20. Source files must be UTF-8 (65be77e, still in 1fc1258)

GCC torture `execute/20000227-1.c` has a raw 0xFF byte inside a string
literal. loomcc stops with `stream did not contain valid UTF-8`. The source
character set is implementation-defined, but 816-tcc and clang accept such
bytes in literals (passing them through unchanged), and Latin-1 bytes in
SNES text strings are plausible. Low priority.

### F24. The interpreter lacks libc functions the ROM has (1fc1258, `ir`)

c-testsuite `00180.c` and tcc tests2 `29_array_address.c` (wrapped
external suites) call `strcpy`: the ROM links PVSnesLib's libc and passes;
`--run-ir` stops with `call to undefined function 'strcpy'`. The interpreter
implements printf/puts/putchar; it would need the mem*/str* functions
PVSnesLib provides as well (harness/libc/string.h lists them).

## Design questions

### Q2. PVSnesLib's `int32_t` under loomcc

devkitsnes's `stddef.h` (and `stdint.h`) choose `typedef long long int
int32_t;` when `__65816__` is defined; under loomcc `long long` is 64 bits.
loomcc now ships its own headers (1fc1258; `t3-sema/headers/stdint-widths.c`
passes), but Loom's build puts devkitsnes/include on the `-I` path. Which
headers win for Loom units is worth checking in the driver.

## Fixed

- **F1 `#line` ignored**: fixed in 1fc1258 (all `t1-pp/line/*` pass).
- **F2 `_Pragma` not implemented**: fixed in 1fc1258.
- **F4, the diagnostic half** (`__VA_OPT__` nested, without `(`, unbalanced,
  outside a variadic macro): fixed in 1fc1258.
- **F5 missing diagnostics for directive constraint violations**: fixed in 1fc1258.
- **F6 `__has_include` gaps**: fixed in 1fc1258.
- **F7 `#define` with no name reported at `<built-in>:0`**: fixed in 1fc1258.
- **F8 lexical errors not diagnosed**: fixed in 1fc1258.
- **F9 a parameter named like a typedef**: fixed in 65be77e.
- **F10 declaration-specifier constraints** (`int char`, `void v;`,
  duplicate members, flexible member not last, 17-bit bit-field, `&register`,
  `int a[0]`, `void *` arithmetic, constant overflow, `= {}`, label at end of
  block, K&R implicit int): fixed in 1fc1258.
- **F11 bit-field layout with mixed unit types**: fixed in 65be77e.
- **F12 negative pointer offsets**: fixed in 65be77e.
- **F13 struct arguments not copied**: fixed in 65be77e.
- **F14 statics with the same name in two units**: fixed in 65be77e.
- **F16 struct by value across the 816-tcc ABI**: fixed in 65be77e.
- **F17 mcpp suite: preprocessor panic on an unterminated call in
  `#include`, and the missing diagnostics**: fixed in 1fc1258, except
  e_ucn (now F23). (mcpp's e_18_4 line 30, `THIS$AND$THAT`, is no longer
  expected to be an error: `$` is an identifier character for loomcc.)
- **F18 the offsetof idiom not folded (Loom's actor.c did not compile)**:
  fixed in 1fc1258; all 31 T6 units compile and assemble.
- **F19 a loop-carried variable assigned late**: fixed in 1fc1258 (or 9fd0628).
- **F15 32-bit multiply/divide/shift and recursion in `rom`**: implemented in
  cac2523; every T4 test passes in `rom` (the remaining UNRESOLVED results
  were emulator starvation, now retried).
- **F21 member of a struct rvalue**: fixed in 83ca7a1.
- **F22 stack overflow on many case labels**: fixed by 83ca7a1 (driver on a
  deep-recursion-safe thread).
- **Q1 32-bit member alignment**: loomcc now aligns 32-bit members to 4
  like 816-tcc (1fc1258); `t3-sema/layout/thirty-two-bit-member.c` passes.

### F25. Re-entrant calls through 816-tcc code corrupt unit locals (rom; build of 2026-09-26 ~08:00)

Test: `t5-snes/interop/reentrant-chain.c`. `unit_step` (loomcc) calls
`tcc_step` (816-tcc) which calls `unit_step` again, three levels deep; each
activation keeps a local (`mine`) live across the call. Expected 213 (host,
host16 and the 816-tcc-only ROM agree); loomcc's ROM fails the CHECK. This is
the case loomcc PLAN section 8 names: a call into foreign code can re-enter
the unit, so a function reachable that way cannot keep its locals in one
static frame (or must save it around external calls).

Fixed in loomcc 179dd28; the tests pass.

### F26. 32-bit results from 816-tcc functions lose their high word (cac2523, rom)

Tests: `t5-snes/interop/i32-across-abi.c` (line 12),
`t5-snes/interop/shared-struct-array.c` (line 17). Calling an 816-tcc
function that returns a 32-bit integer (its `long long`, loomcc's `long`):
`tcc_i32_add(100000, -30000)` comes back as 4464 (70000 & 0xffff),
`tcc_u32_mix(7, 0x12345678, -1)` as 0x567e. The 816-tcc ABI returns the
high word in `tcc__r0h` (DP $02); loomcc reads only `tcc__r0`. (Passing
32-bit arguments to 816-tcc and returning 32-bit results to it both work:
`tcc_calls_scale(-70000)` gives the right value.)

Fixed in loomcc 179dd28; the tests pass.
