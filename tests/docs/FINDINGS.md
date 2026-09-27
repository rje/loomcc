# Findings: loomcc behaviour the suite believes is wrong

Each entry: the test(s), the expected result (with the rule behind it), and
what loomcc did, with the loomcc commit it was seen at. Entries move to
"Fixed" with the commit that fixed them; tests are never edited to match
loomcc. The runner prints the build time of the loomcc binary it ran.

Latest full run: loomcc 481d369 (build of 2026-09-26 12:41 UTC; 810f627 and 69b0b91 change only docs): 2845 results (the tiers plus the wrapped c-testsuite and gcc.dg/cpp), 2813 PASS, 22 FAIL, 10 XFAIL. The 22 are F3, F4, F23 (with the gcc.dg/cpp tests that echo it) and F27; nothing new in T1-T3. Two new code generation findings, F29 and F30, came from rerunning the wrapped tcc tests2 (`130_large_argument.c`, which passed on the build of 08:16 UTC). F20 still reproduces; F24 is fixed.

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

### F28. A function bigger than a ROM bank cannot link (cac2523, rom; low priority)

Csmith seed 239 (`tests/t7-random/run.py --csmith --seeds 239`): loomcc
compiles `func_1` to 34,570 bytes, and wlalink reports `No room for section
"lcc.lcsu0_0_func_1" (34570 bytes) in ROM bank 0`. A 32 KiB LoROM bank is
the hard limit for one SUPERFREE section; loomcc could split huge functions,
limit inlining into them, or at least say which function is too big.
816-tcc cannot assemble this program either (its stack offsets overflow).

### F33. Constraint diagnostics missing, from gcc.dg (481d369)

`external/wrap-gcc-dg-errors.py` wraps 186 of GCC's gcc.dg compile tests
that expect errors (each dg-error becomes `loomcc-diagnostic` at its line;
clang must agree, and must accept the rest of the file under
-pedantic-errors -Werror=vla). loomcc passes 101. Besides F31 and F32, 82
fail because loomcc accepts a line silently. Grouped (first missing line
per test; the list is in the run output):

| group | tests (gcc.dg) | rule |
|---|---|---|
| redeclarations | `decl-2`, `decl-3` (enumerator), `decl-4` (parameter), `redecl-2` (block scope), `redecl-12`, `redecl-13`, `redecl-18`, `redecl-22`, `pr117757-1`, `pr123716`, `pr15360-1`, `noreturn-6` | 6.7p3, 6.2.7 |
| incomplete and invalid types | `array-7`, `pr123461-1`, `pr65050` (array of incomplete element), `pr63549`, `pr69483` (object of incomplete type), `pr27953`, `c99-array-nonobj-1`, `c99-flex-array-*` (6 tests: no named members, in a union, nested, initialised), `struct-empty-3`, `pr67432` (`enum {}`), `c99-tag-4`, `pr14475` (forward enum), `incomplete-typedef-1`, `pr108043` (compound literal of function type), `pr105149`, `pr100532-1` | 6.7.2.1, 6.7.2.2, 6.7.2.3, 6.7.6.2, 6.5.2.5 |
| constant expressions | `case-const-3`, `enum-const-3`, `enum3`, `bitfld-14`, `c99-const-expr-5/6/10`, `c11-static-assert-3`, `c99-init-3`, `c99-intconst-2` (`#if` constant too large) | 6.6, 6.8.4.2, 6.7.2.2 |
| declarations and specifiers | `anon-struct-9`, `anon-struct-15` (duplicate members through anonymous structs), `c11-anon-struct-3`, `declspec-8`, `funcdef-storage-1`, `register-var-3`, `c11-noreturn-5`, `c99-restrict-1/3`, `c99-arraydecl-1/3`, `c99-bool-2`, `c99-impl-int-1`, `c11-parm-omit-1`, `c11-stdarg-1`, `va-arg-4` (`f(...)`), `nested-func-3`, `pr113262` | 6.7, 6.7.3, 6.7.4, 6.9.1 |
| expressions and conversions | `Wincompatible-pointer-types-5`, `diag-aka-4`, `c99-func-4` (`char *p = __func__`), `lvalue-7`, `pointer-arith-4/8` (void arithmetic and `sizeof(void)`), `bitfld-12` (`offsetof` a bit-field), `c99-array-lval-5`, `c11-generic-2` (two defaults), `pr45750`, `Wreturn-mismatch-3`, `pr29521-2` (`return` with a void expression) | 6.5, 6.5.16.1, 6.8.6.4 |
| other | `c11-uni-string-2` (`L"a" u8"b"`), `c11-static-assert-8` (C23 form), `c11-binary-constants-2`, `c99-init-2` (GNU range designator), `extra-semi-3`, `large-size-array`, `large-size-array-3`, `vla-18`, `pr30551-3` (`void main(char)`) | various; the last group is partly quality, not constraint |

Some of these overlap F27. None changes code generation for valid code;
they matter for users who expect loomcc to catch their mistakes the way gcc
and clang do. The tests are run with
`./run-tests external/fetched/gcc-dg-errors-wrapped` (after
`external/fetch.sh gcc-dg && external/wrap-gcc-dg-errors.py`).

### F20. Source files must be UTF-8 (65be77e, still in 1fc1258)

GCC torture `execute/20000227-1.c` has a raw 0xFF byte inside a string
literal. loomcc stops with `stream did not contain valid UTF-8`. The source
character set is implementation-defined, but 816-tcc and clang accept such
bytes in literals (passing them through unchanged), and Latin-1 bytes in
SNES text strings are plausible. Low priority.

### F36. Defining a variadic function is not supported (open; low priority for Loom)

`va_start`/`va_arg`/`va_end` report "variadic functions are not supported by
the 65816 backend". Calling variadic functions compiled by 816-tcc (printf)
works. Loom's C defines none.

## Design questions

### Q2. PVSnesLib's `int32_t` under loomcc

devkitsnes's `stddef.h` (and `stdint.h`) choose `typedef long long int
int32_t;` when `__65816__` is defined; under loomcc `long long` is 64 bits.
loomcc now ships its own headers (1fc1258; `t3-sema/headers/stdint-widths.c`
passes), but Loom's build puts devkitsnes/include on the `-I` path.
Answered in 481d369: loomcc's headers win under Loom's include order;
`t6-loom/run/q2-int32-widths.c` checks it with the T6 drivers' options.

### Q3. Pointers that step across the $7E/$7F WRAM boundary

`t5-snes/data/wram-bank-crossing.c` (XFAIL). WRAM is one 128 KiB block, so
a pointer walking up from `$7E:FFF8` could continue into `$7F:0000`. loomcc
(since bfdacc7, "no bank copy on pointer steps") increments only the low 16
bits, so `p` after 16 steps is `$7E:0008`; 816-tcc keeps the pointer right
but its stores through `*p++` wrap within bank `$7E`. Neither can happen for
an object the toolchain lays out (no section spans a bank), so this matters
only for code that addresses WRAM absolutely, as some engines do for big
buffers. Worth a sentence in loomcc's documentation either way.

## Fixed

- **F24 the interpreter lacked libc functions** (`strcpy` in c-testsuite
  `00180.c` and tcc tests2 `29_array_address.c` under `--run-ir`): fixed in
  481d369; both pass.
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

### F29. Stack-relative offsets past 255 wrap (rom; release build of 08:16 UTC and 481d369)

Test: `t4-exec/call/struct-arg-300-bytes.c` (CHECK at line 15; ir, host,
host16 and 816-tcc's ROM pass). A 300-byte struct passed by value: the
callee copies its parameter from the stack with `lda n,s`, and the 65816's
stack-relative mode takes an 8-bit offset. loomcc emits the offset modulo
256 (`lda 254,s` is followed by `lda 0,s`, `lda 2,s` ... for bytes 256 and
up of the argument area), so everything past the first 252 bytes of
arguments is read from the wrong place. Silent wrong code. It needs a
different addressing path once the offset passes 255 (`tsc`, add, and a
direct-page or long-indirect copy, or a block move). Loom passes nothing
this large, so it is low risk for Loom, but any function with more than
about 250 bytes of parameters is affected.

Fixed in loomcc e247f27: the prologue reads arguments through a long
pointer once the argument area passes 255 bytes, and a whole-struct push
to an 816-tcc callee copies through `[$1c],y`. The tests pass.

### F30. The frame save around foreign calls copies the whole unit, one word at a time (rom; 481d369)

Tests: `t5-snes/interop/big-frame-foreign-call.c` (does not link:
`No room for section "lcc.loomcc_test_main" (48120 bytes)`), and tcc tests2
`130_large_argument.c` (wrapped external suite; it passed on the build of
08:16 UTC). The F25 fix saves the unit's static frame area on the hardware
stack around every call into foreign code (here printf, compiled by
816-tcc). The save is the entire unit's `lcc.cstack` area (every function's
frame, 10,778 bytes in 130_large_argument), not the caller's live slots, and
it is unrolled: `lda.w`/`pha` per word before the call and `pla`/`sta.w`
per word after it, about 8 bytes of code per word per call site. One
function with a 6,000-byte local array makes `main`, which has almost no
locals of its own, 48 KB of code for two printf calls. The same save puts
that many bytes on the SNES hardware stack in bank 0, which a stack in low
RAM cannot hold for frames of this size. Suggestions: save only the frames
of functions that can be re-entered (the ones reachable from the callee's
callbacks) and only their live slots; save with a loop or `mvn` block move
to a separate save stack rather than the hardware stack. Loom's Cliffside
build works (its frames are small), so this is a scaling problem; it
turns into a link failure or a stack overflow as units grow.

Fixed in loomcc e247f27: only the caller's own frame words live across
the call are saved (every active member of a recursive component saves
its own frame), with pushes for 16 bytes or less and an `mvn` block move
beyond; a save over 1024 bytes is an error diagnostic, not silent stack
use; libc functions are known not to call back. The tests pass (and
`130_large_argument.c` again).

### F34. A struct-returning call loses a scalar argument staged in $00 (rom; b63113c)

Found by `tests/t7-random/run.py --shapes --small --no-foreign` (seed 7:
no big frames, no foreign calls, so neither F29 nor F30) and still failing
after their fix. C-Reduce took it to 579 bytes with an out-of-bounds store it
introduced; the hand-cleaned form is
`tests/t4-exec/call/struct-return-with-scalar-argument.c`. The IR interpreter
passes; the ROM fails.

For `f = h(f, a)`, where `E h(E i, u16 k)` returns a struct, the caller
emits

```
  lda.w a          ; k staged in $00
  sta.b $00
  ...              ; struct argument copied into h's frame
  lda.w #lcc_cstack_u0
  sta.b $1c        ; hidden result pointer
  ...
  lda.b $1c
  sta.b $00        ; result pointer into $00/$02: overwrites k
  lda.b $1e
  sta.b $02
  lda.b $00
  sta.b $04        ; "k" into its argument slot: now the frame address
  jsl lcb_h
```

The argument moves into $00-$04 are a parallel copy done in sequence
without checking that a source is also a destination. With `g.d != 0` in
place of `g.d || 0` in h, h's frame layout changes, k is no longer staged
in $00, and the program passes, so the failure depends on where the
register allocator puts the staged value. The fix belongs where the call's
argument moves are ordered: sequence the moves (or go through a temporary)
so that none overwrites a source still to be read.

Fixed in loomcc aeeb0c2: the hidden result pointer is formed in scratch
and stored after the parallel argument move; index-register arguments
whose direct-page source the move overwrites are parked on the stack; an
816-tcc result homed on its own high word ($02 or $04) is stored high word
first. The test passes.

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

Fixed in loomcc 0da033c: all three are diagnosed (a missing variadic
argument only when the macro body does not use `__VA_OPT__` or
`, ## __VA_ARGS__`, which are written for the empty case).

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

Fixed in loomcc 0da033c: errors for the array of void, auto/register at
file scope, the repeated enumerator, restrict on a non-pointer and the
linkage conflict; warnings for the empty struct, `sizeof(void)` and the
distinct pointer comparison. The tests pass.

### F31. A panic on an enumerator of LLONG_MAX (481d369; high priority, a crash)

Tests: `t3-sema/constraint/enum-value-too-large.c`,
`enum-value-too-large-then-next.c` (and gcc.dg `c11-enum-1.c`).
`enum big { BIG = 9223372036854775807LL };` makes loomcc panic at
`crates/sema/src/check.rs:414:17: attempt to add with overflow`, even with
no enumerator after it. Expected: a diagnostic (6.7.2.2p2: the value must be
representable as an int; loomcc already warns for values that are merely
too big for 16 bits).

Fixed in loomcc 0da033c: an implicit enumerator after the largest value is
an overflow error, not an arithmetic panic. The tests pass.

### F32. Two scope rules give spurious errors (481d369)

- `t3-sema/scope/tag-in-parameter-list.c` (from gcc.dg `struct-in-proto-1.c`):
  `int f(struct S { int i; } s) { return sizeof(struct S); }`: a tag
  declared in a function definition's parameter list is in scope, and
  complete, in the body (6.2.1p4). loomcc: `invalid application of 'sizeof'
  to an incomplete type 'struct S'`.
- `t3-sema/scope/inner-extern-composite-type.c` (from gcc.dg `redecl-14.c`):
  an inner-block `extern IA5 *a[];` completes the element type for that
  scope (6.2.7p4 composite type); loomcc keeps the file-scope `IA *` and
  rejects `sizeof(*a[0])`. 816-tcc does the same. Esoteric.

Also seen: after a real error (`void foo(); int foo[] = {0};`, gcc.dg
`pr69819.c`) loomcc adds a spurious second error on the earlier line
(`variable has incomplete type 'void ()'`). Cosmetic.

Fixed in loomcc 0da033c (both scope rules; composite types now recurse
through pointers and array elements). The cosmetic second error after
`pr69819.c`'s real one remains.

### F35. Compiler crashes found by sweeping 14,304 C files (0da033c)

`loomcc -fsyntax-only` and `-S` over every C file in this suite and the
fetched GCC testsuites (gcc.dg, gcc.c-torture) found 63 crashes and 8
timeouts, all in inputs outside Loom's subset:

| class | example | fix |
|---|---|---|
| stack overflow (21 files): `va_arg` of a struct type recursed between the aggregate and lvalue lowerings | `931004-2.c` | an error (variadic functions are unsupported, F36) |
| shift-count underflow (16): a `long long` bit-field wider than 32 bits | `bitfld-3.c`, `920501-3.c` | an error |
| `#line 18446744073709551616` overflowed the next line number | `cpp/line6.c` | clamped (the warning stays) |
| initialising a flexible array member wrote past the object | `pr56078.c` | an error (a GNU extension) |
| a 6x10^17-byte array aborted in allocation | `pr65680.c` | an error: no object may exceed the 16 MiB address space |
| a switch case range of 2^64 overflowed | `pr34154.c` | 128-bit arithmetic |
| `-fsyntax-only` quadratic in file-scope declarations (over 60 s) | `limits-externdecl.c` | globals indexed by name (2.5 s) |

Tests: `t3-sema/robust/*.c`, `t5-snes/unsupported/*.c`.

Fixed in loomcc d1ab81f.

### F37. Compile time on huge functions (open; low priority)

The debug build takes 90 s at `-S` for `gcc.c-torture/compile/20001226-1.c`
and over 120 s for `limits-caselabels.c` (10,000 case labels): the backend's
per-instruction liveness queries are quadratic in function size. Loom's
largest functions compile in well under a second.

Fixed in loomcc F37HASH: branch relaxation relaxed one branch per pass
over the whole function (quadratic); it now relaxes every out-of-range
branch in each pass, reaching the same fixed point (the M9 ROMs are
byte-identical). Release build: `20001226-1.c` 83 s to 7.9 s,
`limits-caselabels.c` over 120 s to 9.9 s, `pr43058.c` 12.5 s. An array
sized by its initializer is now bounded by the 16 MiB address space too
(`large-size-array-2.c` laid out an 8 GiB object). Still slow:
`limits-fndefn.c`, a function with 100,000 parameters (over 10 minutes).
