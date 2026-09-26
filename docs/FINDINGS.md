# Findings: loomcc behaviour the suite believes is wrong

Each entry: the test(s), the expected result (with the rule behind it), and
what loomcc did, with the loomcc commit it was seen at. Entries move to
"Fixed" when the tests pass; tests are never edited to match loomcc.

Run: `./run-tests --tier t1` against `/Users/rje/src/rust/loomcc/target/debug/loomcc`.

## Open

### F1. `#line` is ignored (loomcc 2ee694c)

Tests: `t1-pp/line/line-directive*.c`, `line-moves-diagnostics.c`,
`line-file-moves-diagnostics.c`, `line-then-include.c`, `line-escapes-file.c`.

Expected: `#line 100` makes the next line 100 (C17 6.10.4p3); `#line 200
"renamed.c"` also changes `__FILE__` (p4); operands are macro-expanded (p5);
the digit sequence is decimal (`#line 010` gives 10); later diagnostics use
the new line and file. Malformed forms (`#line`, `#line abc`, `#line 0x10`,
`#line 10 file.c`) are errors; `#line 0` and `#line 2147483648` deserve a
diagnostic.

loomcc: `__LINE__` and `__FILE__` are unchanged, every malformed form is
accepted silently, diagnostics keep the physical line.

### F2. `_Pragma` is not implemented (2ee694c)

Tests: `t1-pp/pragma/operator*.c`, `pragma/in-macro-arg.c`,
`std/c17-6.10.9-pragma-ex.c`.

Expected: `_Pragma("foo bar")` is destringized and executed as `#pragma foo
bar` (C17 6.10.9), which `-E` prints on its own line; `\"` and `\\` are
unescaped and an `L` prefix dropped; `_Pragma x`, `_Pragma(foo)` and an
unterminated `_Pragma("foo"` are errors.

loomcc: `_Pragma ( "foo bar" )` passes through as ordinary tokens; no errors.

### F3. Pure hide sets over-paint in deferred expansion (2ee694c)

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
Loom's C does not use it, so this is low priority, but it is a conformance
difference in the algorithm the plan names.

### F4. `__VA_OPT__` details (2ee694c)

Tests: `t1-pp/variadic/va-opt-*.c`.

- `va-opt-c23-examples.c`, `va-opt-empty-expansion.c`: `F(EMP)` with
  `#define EMP` must give `f(0)` (C23 6.10.5.1 EXAMPLE: `__VA_OPT__` tests
  whether the variable arguments *expand* to nothing); loomcc gives `f(0,)`.
- `va-opt-paste.c`: `#define H4(X, ...) __VA_OPT__(a X ## X) ## b` with
  `H4(, 1)` must give `a b` (the `__VA_OPT__` result ends in a placemarker,
  and that is what `## b` pastes with); loomcc gives `ab`.
- `va-opt-stringize.c`: `#__VA_OPT__(...)` must stringize the replacement
  (`H3(, 0)` gives `""`); loomcc leaves `#` in the output.
- `va-opt-nested.c`, `va-opt-no-paren.c`, `va-opt-unbalanced.c`,
  `va-opt-not-variadic.c`: `__VA_OPT__` inside `__VA_OPT__`, without `(`,
  unbalanced, or in a non-variadic macro must be diagnosed; loomcc accepts
  them.

### F5. Missing diagnostics for constraint violations in directives (2ee694c)

Each is a constraint (or syntax rule) that requires a diagnostic:

| test | rule | loomcc |
|---|---|---|
| `object/whitespace-after-name.c` (`#define X-1`) | 6.10.3p3 | silent |
| `function/duplicate-param.c` (`#define f(a, a)`) | 6.10.3p6 | silent |
| `stringize/hash-not-param.c`, `hash-at-end.c` | 6.10.3.2p1 | silent |
| `variadic/va-args-outside.c`, `va-args-in-text.c`, `define-va-args.c` | 6.10.3p5 | silent |
| `cond/comma-operator.c` (`#if (1, 2)`) | 6.6p3 | silent |
| `cond/overflow.c` (`#if 0x7fffffffffffffff + 1`) | 6.6p4 | silent |
| `cond/endif-extra-tokens.c`, `else-extra-tokens.c`, `ifdef-extra-tokens.c`, `directive/undef-extra.c`, `include/extra-tokens.c`, `line/line-directive-extra.c` | directive grammar (6.10) | silent |
| `include/unterminated-angle.c` (`#include <angle.h`) | 6.10.2 | silent (and the include is performed) |
| `object/undef-defined.c` | 6.10.8p2 (undefined; clang errors) | silent; `#define defined` is rejected, `#undef defined` is not |

### F6. `__has_include` gaps (2ee694c)

- `include/has-include.c`: `#ifdef __has_include` must be true (C23 6.10.1:
  it behaves as a defined macro name for `#ifdef` and `defined`); loomcc says
  it is not defined.
- `include/has-include-macro.c`: `__has_include(HDR)` with
  `#define HDR "h/a.h"` must macro-expand the operand; loomcc returns false.
- `include/has-include-outside-if.c`: `__has_include` outside `#if`/`#elif`
  is an error; loomcc accepts it.

### F7. Diagnostic location for `#define` with no name (2ee694c)

Test: `directive/define-no-name.c`. Expected an error at the directive's
line; loomcc reports `<built-in>:0: error: macro name missing`.

### F8. Lexical errors not diagnosed (2ee694c)

- `lex-errors/unterminated-string.c`, `unterminated-char.c`: an unmatched `"`
  or `'` (undefined behaviour by 6.4p3, diagnosed by every compiler here);
  loomcc passes the fragment through silently.
- `lex-errors/empty-char.c`: `''` is not a character constant; silent.
- `lex-errors/backslash-at-eof.c`: a file ending in backslash-newline
  (5.1.1.2p2); silent (clang warns). No crash, which is the main point.

### F9. A parameter may not share a name with a typedef (loomcc working tree, 2025-09-25 23:18 build)

Test: `t2-parse/typedef/param-shadows-typedef.c`.

```c
typedef char T;
int f(int T) { return T * 2; }
```

Expected: accepted; `T` is an ordinary identifier inside `f` (C17 6.2.1:
the parameter's scope hides the typedef name). loomcc: `error: expected ')'
before 'T'`. Loom does not do this today, but hooks written by users could.

### F10. Declaration-specifier constraints (same build)

| test | expected | loomcc |
|---|---|---|
| `t2-parse/errors/two-types.c` (`int char x;`) | error, 6.7.2p2 | accepted |
| `t3-sema/constraint/void-object.c` (`void v;`) | error, 6.7p7/6.9.2 (incomplete type) | accepted |
| `t3-sema/constraint/duplicate-member.c` | error, 6.7.2.1 | accepted |
| `t3-sema/constraint/flexible-member-not-last.c` | error, 6.7.2.1p3 | accepted |
| `t3-sema/constraint/bitfield-wider-than-int.c` (`unsigned too_wide : 17;` in a struct; 16-bit int) | error, 6.7.2.1p4 | the struct member is accepted (the file-scope case is rejected) |
| `t3-sema/constraint/address-of-register.c` (`&r`, `register int r`) | error, 6.5.3.2p1 | accepted |
| `t3-sema/constraint/zero-array-size.c` (`int a[0];`) | diagnostic, 6.7.6.2p1 | accepted |
| `t3-sema/constraint/void-pointer-arithmetic.c` (`p + 1`, `void *p`) | diagnostic, 6.5.6p2 | accepted |
| `t3-sema/consttype/overflow-diagnosed.c` (`int a = 32767 + 1;`) | diagnostic, 6.6p4 | accepted (the other two lines are diagnosed) |
| `t2-parse/init/empty-braces-error.c` (`int a[2] = {};`) | diagnostic (C17 grammar) | accepted |
| `t2-parse/stmt/label-at-end-of-block.c` (`end: }`) | diagnostic (C17 grammar; C23 allows it) | accepted |
| `t2-parse/decl/kr-undeclared-param.c` (`int neg(x) { ... }`) | diagnostic (no implicit int since C99) | accepted |

### F11. Bit-field layout differs from 816-tcc for mixed unit types (same build)

Test: `t3-sema/layout/bitfields-816tcc.c`, line 19:
`struct P { unsigned char a : 4; unsigned b : 4; };` is 4 bytes in 816-tcc
(`scripts/tcc-layout.py`: `a` in a char unit at byte 0, `b` in a new 16-bit
unit at byte 2). loomcc makes it 2 bytes (b packed into the same bytes as a). The other seven structs in the
file match.

### F12. Negative pointer offsets read the wrong element (same build, `rom` mode)

Tests: `t4-exec/pointer/arithmetic.c` line 11 (`*(q - 2)` where `q = &a[9]`),
`t4-exec/array/index-types.c` line 15 (`mid[s8]` with `i8 s8 = -3`,
`mid = &a[150]`). Expected 7 and 147 (host, host16 and the 816-tcc ROM
agree). loomcc's ROM computes a different element, so a negative offset is
probably treated as unsigned (a 16-bit offset added to a 24-bit pointer
without borrowing from the bank byte, or zero-extended from 8 bits).

### F13. Struct arguments are not copied (same build, `rom` mode)

Test: `t4-exec/struct/pass-by-value.c`, line 10. `sum(struct P p)` does
`p.x += 100`; afterwards the caller's `a.x` must still be 3. In loomcc's ROM
it is changed: the callee writes the caller's object.

### F14. Statics with the same name in two units collide (same build, `rom` mode)

Test: `t4-exec/global/static-same-name-two-units.c`, line 9. Both units name
their `static short hidden` `lcs0_hidden`, so the second unit reads the
first unit's variable (1, not 2). 816-tcc avoids this with
`tccs_{WLA_FILENAME}_name`; loomcc needs a per-unit prefix too.

### F15. Not yet supported (tracked, not bugs)

`rom` mode rejects 32-bit multiply, divide and shift, and recursion
(`recursion is not supported yet (static frames)`): `t4-exec/arith32/*`,
`t4-exec/call/recursion-*.c`, `mutual-recursion.c`, `misc/fixed-point.c`.
`--run-ir` does not exist yet, so `ir` results are UNSUPPORTED.

## Design questions

### Q1. Alignment of 32-bit members

`t3-sema/layout/thirty-two-bit-member.c` (XFAIL). 816-tcc aligns its 32-bit
integer (`long long`) to 4 inside structs (`struct { char c; long long l; }`
is 8 bytes); loomcc aligns its 32-bit `long` to 2 (6 bytes). Code that
shares a 32-bit field between the two compilers through a typedef (like
`loomcc-test.h`'s `i32`) would disagree. Loom has no 32-bit fields today.

### Q2. PVSnesLib's `int32_t` under loomcc

devkitsnes's `stddef.h` (and `stdint.h`) choose `typedef long long int
int32_t;` when `__65816__` is defined. loomcc defines `__65816__` and makes
`long long` 64 bits, so `int32_t` would be 64 bits under loomcc with the
PVSnesLib headers Loom builds with. The same header also typedefs
`int16_t` twice (`short int`, then `int`), which a conforming compiler must
reject. loomcc probably needs its own freestanding headers (or `-isystem`
overrides) rather than devkitsnes's.

## Fixed

(none yet)
