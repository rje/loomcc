# Reference-tool divergences

Where a reference tool disagrees with a test's expectation, the test carries
`// loomcc-ref-diverges: <tool> [<code>] <reason>` and the runner reports that
reference result as XFAIL (and as XPASS if the tool ever starts agreeing, so
this list cannot go stale silently). This file explains the codes; the index
at the end lists every test per code and is regenerated with
`scripts/divergences.py`.

The expected results follow the C standard. Where the standard leaves a
choice to the implementation, the test says which choice loomcc makes and
why (usually: the same as 816-tcc, because Loom's ROMs and assembly can
observe it).

## clang

clang (Apple clang 17, `-std=c17 -pedantic`) agrees with every T1
expectation except:

| code | what | affects |
|---|---|---|
| `clang-dot-slash` | `__FILE__` in a header found beside a file in the current directory is spelled `"./h/file.h"`; gcc and 816-tcc print `"h/file.h"`. Implementation-defined; loomcc follows 816-tcc so that asserts and log strings match between the two compilers. | `-E` and `__FILE__` strings |

Behaviours of clang that the suite deliberately does not test (see
docs/PLAN.md): the GNU comma swallow `, ## __VA_ARGS__` (clang applies it even
in `-std=c17`), macro invocations whose arguments run past the end of an
included file (clang accepts them; gcc rejects them), `defined` produced by
macro expansion.

## 816-tcc

816-tcc 0.9.25's preprocessor (PVSnesLib's devkitsnes) is old TinyCC and
diverges widely. Two classes matter differently:

- **`-E` printing only** (does not change what 816-tcc compiles):
  `tcc-E-glue`, `tcc-char-spelling`, `tcc-no-pragma-E`, `tcc-no-pragma-op`
  (in `-E` output). Also: `816-tcc -E` always dies with SIGABRT while exiting,
  after writing complete output; the runner treats that as success when no
  error was printed.
- **Real preprocessing differences** (they change what 816-tcc compiles, so
  code that relies on them builds differently with loomcc): everything else.
  The ones Loom-style code could plausibly hit are `tcc-if-arith` (#if
  arithmetic is not done in intmax_t: `#if 65535 + 1 == 65536` is false),
  `tcc-blue-paint` and `tcc-memory-full` (rescanning bugs),
  `tcc-stringize-space` (spaces in `#x` results), `tcc-line-octal`,
  `tcc-no-counter` and `tcc-c99` (`__STDC_VERSION__` is 199901L).
  Loom's C uses none of these today (loomcc PLAN section 3: one `##` paste,
  no `#`, no variadics).
- **Missing diagnostics**: 816-tcc accepts many constraint violations
  silently (`tcc-no-redef-diag`, `tcc-lax-params`, `tcc-extra-tokens`,
  `tcc-if-lax`, `tcc-va-args-anywhere`, `tcc-define-defined`,
  `tcc-line-lax`) and reports many errors one line late
  (`tcc-line-after`: it has already read the newline).

| code | what |
|---|---|
| `tcc-E-glue` | `-E` drops the space between tokens that were separated by a comment or came from different expansions: `+EMPTY+` prints `++` |
| `tcc-char-spelling` | `-E` respells character literals (`'"'` as `'\"'`), also inside stringized text |
| `tcc-no-digraphs` | no digraphs (`<:` `%:` ...) |
| `tcc-no-dollar` | `$` is not an identifier character |
| `tcc-no-ucn` | no universal character names in identifiers |
| `tcc-no-u8` | no `u8` string prefix |
| `tcc-no-trigraphs` | no trigraphs (neither has loomcc, by design) |
| `tcc-dotdot` | `..` (two dots) is rejected |
| `tcc-stray` | a stray `\` or `@` outside a literal is an error even in `-E` |
| `tcc-hash-midline` | a `#` in the middle of a line (after a macro that expands to nothing, or after other tokens) is treated as a directive |
| `tcc-hashhash-lex` | `###`, and `#` passed as a macro argument to `##`, lose tokens |
| `tcc-hashhash-stringize` | the `##` formed by `# ## #` disappears when stringized (C17 6.10.3.3p4 example) |
| `tcc-blue-paint` | a self-referential object-like macro used inside another expansion is expanded again (`z[0][0][0]` in C17 6.10.3.5 EXAMPLE 3) |
| `tcc-memory-full` | "memory full" when a macro's replacement ends in a partial invocation completed from the source (`#define g f(` then `g 3)`) |
| `tcc-stringize-space` | `#x` drops the space a comment stood for and misplaces spaces around expanded tokens |
| `tcc-no-redef-diag` | incompatible macro redefinitions are accepted silently (C17 6.10.3p2) |
| `tcc-no-c99-ws` | no diagnostic for a missing space after an object-like macro's name |
| `tcc-lax-params` | malformed parameter lists, `#` not followed by a parameter, `##` at either end, `...` not last: all accepted |
| `tcc-define-defined` | `#define defined` / `#undef defined` accepted |
| `tcc-unterminated-invocation` | an unterminated invocation at end of file is reported as "too many args" on another line |
| `tcc-va-args-anywhere` | `__VA_ARGS__` accepted outside a variadic macro |
| `tcc-no-va-opt` | no `__VA_OPT__` (C23) |
| `tcc-gnu-comma` | `, ## __VA_ARGS__` with an empty argument drops the comma (GNU), standard C keeps it |
| `tcc-if-arith` | `#if` arithmetic uses 816-tcc's own int widths, not intmax_t/uintmax_t (C17 6.10.1p4): `32767 + 1 > 0`, `-1 > 0u`, `(1 ? -1 : 0u) > 0` all come out wrong |
| `tcc-if-eager` | division by zero is diagnosed in unevaluated `#if` operands (`1 || 1/0`) |
| `tcc-if-lax` | malformed `#if` expressions accepted (assignment, strings, floating constants, commas, overflow) |
| `tcc-line-after` | errors in directives are reported on the following line |
| `tcc-missing-endif-eof` | a missing `#endif` is reported at the end of the file, not at the `#if` |
| `tcc-extra-tokens` | extra tokens after `#endif`, `#else`, `#ifdef`, `#include`, `#undef`, `#line` are ignored silently |
| `tcc-no-has-include` | no `__has_include` |
| `tcc-no-pragma-once` | `#pragma once` is ignored |
| `tcc-no-pragma-E` | `-E` drops `#pragma` lines |
| `tcc-no-pragma-op` | `_Pragma(...)` produces nothing in `-E` output |
| `tcc-line-octal` | `#line 010` sets line 8 (the digit sequence is decimal: 10) |
| `tcc-line-lax` | `#line 0`, `#line 0x10`, `#line 10 file.c` accepted |
| `tcc-line-macro-hang` | `#line N F` with macro operands never terminates |
| `tcc-no-counter` | no `__COUNTER__` |
| `tcc-c99` | `__STDC_VERSION__` is `199901L` |
| `tcc-no-hosted` | no `__STDC_HOSTED__` |
| `tcc-unknown-directive` | an unknown directive (`#foo`) in an active group is ignored silently |
| `tcc-skipped-apostrophe` | an unmatched `'` in a skipped group is an error |
| `tcc-lex-lax` | unterminated string/character literals pass through `-E` silently |
| `tcc-no-paste-diag` | an invalid `##` result (`+ ## -`) is not diagnosed |

## 816-tcc as a compiler (T2-T5)

The compile and execute references use 816-tcc's front end and code
generator. Codes:

| code | what | affects |
|---|---|---|
| `tcc-long16` | `long` is 16 bits (loomcc: 32); `long long` is 32 | every test using `long`; the harness maps `i32` to `long long` for 816-tcc |
| `tcc-fold-host-int` | the constant folder does `unsigned int` arithmetic in 32 bits: `-1 + 0u != 65535u`, `-1 / 2u` folds to 0x7fffffff, `(i16)(u16)65535u` folds to 65535, while the same operations at run time are 16-bit | **real code**: a constant expression and the same expression on variables can disagree in 816-tcc builds |
| `tcc-deref-call-spill` | `(*f)(a, b)` through a function pointer local: after pushing the arguments 816-tcc stores `f` into its stack slot with an S-relative offset that ignores the pushes, overwriting an argument, then calls through a garbage bank | **real code**: calls written `(*fp)(...)` can crash 816-tcc builds (plain `fp(...)` is fine) |
| `tcc-for-empty-cond` | `for (init;; step)` with no condition but a step expression: 816-tcc emits `__local_0: __local_1: jmp __local_0` before the body, so the loop spins forever without running the body (`for (;;)` without a step and `while (1)` are fine) | **real code**: hangs 816-tcc builds |
| `tcc-no-llshift` | variable 32-bit shifts call `tcc__ashldi3`, which PVSnesLib's libtcc does not provide (link error) | 32-bit shifts by a variable |
| `tcc-crash` | 816-tcc crashes on `sizeof (x += 1)` | |
| `tcc-no-for-decl` | no declarations in `for (...)` (C99) | tests keep loop counters at block scope for 816-tcc's sake |
| `tcc-no-array-static`, `tcc-no-static-assert`, `tcc-no-c11-align`, `tcc-no-noreturn`, `tcc-no-generic`, `tcc-empty-init` | missing C99/C11 features, or GNU `= {}` accepted | |
| `tcc-tag-scope` | a struct tag redeclared in an inner block is rejected as a redefinition | |
| `tcc-lax-decl`, `tcc-lax-switch`, `tcc-lax-types`, `tcc-lax-init`, `tcc-lax-register`, `tcc-lax-sizeof`, `tcc-lax-return`, `tcc-zero-array`, `tcc-void-arith`, `tcc-implicit-int`, `tcc-implicit-function`, `tcc-no-overflow-diag` | constraint violations 816-tcc accepts silently (duplicate members, conflicting types, duplicate `case`, void objects, `&register`, `sizeof` of a bit-field or function, `return;` in an int function, `int a[0]`, `void *` arithmetic, implicit int and implicit declarations, constant overflow) | diagnostics only |

GCC torture execute tests that 816-tcc gets wrong are listed, with reasons,
in `external/gcc-torture-execute.tcc-xfail` (an `--xfail-list` for the
`tcc-rom` reference): 21 of the 450 selected tests, several of them because
816-tcc's `long` is 16 bits.

clang with `--target=msp430-none-elf` (`clang16`, `host16`) differs from the
SNES target in pointer size (`clang16-ptr16`: 2 bytes, not 4) and in plain
`char` signedness (pinned with `-fsigned-char`). `harness/host16/fixup.py`
works around the LLVM interpreter's gaps under `host16`: it ignores `byval`
(a callee's writes to a struct parameter would reach the caller's object),
it keeps host pointers in memory (so pointers are widened and aggregate copy
lengths recomputed), it zero-extends narrow GEP indices, it has no
`freeze`, and its `ptrtoint` to i16 does not truncate the address.

## Execute references

`host`, `host16` and `tcc-rom` divergences are declared per test; the
harness self-tests in `t4-exec/smoke/xfail-*` diverge everywhere on purpose
(they prove that abort() and a non-zero return are detected).

## Index (generated by scripts/divergences.py)

### `clang-dot-slash` (clang, 2 tests)

clang spells it "./h/file.h"

- `t1-pp/include/file-in-header.c`
- `t1-pp/line/line-then-include.c`

### `clang16-bitfield-units` (clang16, 1 test)

clang packs bit-fields of different declared types into shared bytes

- `t3-sema/layout/bitfields-816tcc.c`

### `clang16-long-align` (clang16, 1 test)

msp430 aligns long to 2

- `t3-sema/layout/thirty-two-bit-member.c`

### `clang16-ptr16` (clang16, 7 tests)

msp430 pointers are 2 bytes

- `t3-sema/layout/function-pointer-member.c`
- `t3-sema/layout/nested-struct-alignment.c`
- `t3-sema/layout/pointer-array-member.c`
- `t3-sema/layout/pointer-first-last.c`
- `t3-sema/layout/pointer-member.c`
- `t3-sema/layout/union-with-pointer.c`
- `t3-sema/promote/pointer-size.c`

### `uncoded` (host, 3 tests)

deliberately calls abort()

- `t4-exec/smoke/xfail-harness-detects-abort.c`
- `t4-exec/smoke/xfail-harness-detects-nonzero.c`
- `t4-exec/stdio/output-mismatch-detected.c`

### `uncoded` (host16, 3 tests)

deliberately calls abort()

- `t4-exec/smoke/xfail-harness-detects-abort.c`
- `t4-exec/smoke/xfail-harness-detects-nonzero.c`
- `t4-exec/stdio/output-mismatch-detected.c`

### `tcc-E-glue` (tcc, 7 tests)

816-tcc -E drops the space between separate tokens

- `t1-pp/lex/comment-is-space.c`
- `t1-pp/lex/comment-slashes-in-block.c`
- `t1-pp/lex/print-no-paste-args.c`
- `t1-pp/lex/print-no-paste-dot.c`
- `t1-pp/lex/print-no-paste-empty.c`
- `t1-pp/lex/print-no-paste-plus.c`
- `t1-pp/rescan/alternating-invocations.c`

### `tcc-array-decay-sizeof` (tcc, 1 test)

816-tcc gives sizeof(a + 0) the array's size, not a pointer's

- `t3-sema/expr/array-decay-in-sizeof-and-ops.c`

### `tcc-blue-paint` (tcc, 1 test)

816-tcc re-expands z (z[0][0][0]) and then fails with "memory full" on `h 5)`

- `t1-pp/std/c17-6.10.3.5-ex3.c`

### `tcc-c99` (tcc, 2 tests)

816-tcc reports 199901L

- `t1-pp/predef/stdc-in-if.c`
- `t1-pp/predef/stdc-version.c`

### `tcc-char-spelling` (tcc, 3 tests)

816-tcc -E respells character literals ('"' as '\"')

- `t1-pp/lex/char-literals.c`
- `t1-pp/stringize/embedded-quote-in-char.c`
- `t1-pp/stringize/escape-chars.c`

### `tcc-crash` (tcc, 1 test)

816-tcc crashes on sizeof of an assignment expression

- `t3-sema/promote/compound-assign-types.c`

### `tcc-define-defined` (tcc, 3 tests)

816-tcc lets `defined` be defined or undefined

- `t1-pp/object/define-defined.c`
- `t1-pp/object/undef-defined.c`
- `t1-pp/predef/undef-predefined.c`

### `tcc-dotdot` (tcc, 1 test)

816-tcc rejects `..` (two dots) as a token pair

- `t1-pp/lex/max-munch.c`

### `tcc-empty-init` (tcc, 1 test)

816-tcc accepts `{}`

- `t2-parse/init/empty-braces-error.c`

### `tcc-extra-tokens` (tcc, 6 tests)

816-tcc ignores tokens after #endif/#else/#ifdef

- `t1-pp/cond/else-extra-tokens.c`
- `t1-pp/cond/endif-extra-tokens.c`
- `t1-pp/cond/ifdef-extra-tokens.c`
- `t1-pp/directive/undef-extra.c`
- `t1-pp/include/extra-tokens.c`
- `t1-pp/line/line-directive-extra.c`

### `tcc-fold-host-int` (tcc, 7 tests)

816-tcc folds (u16)-1 without truncating to 16 bits

- `t3-sema/constexpr/casts-in-constant-expressions.c`
- `t3-sema/promote/conditional-operator.c`
- `t3-sema/promote/int-plus-unsigned.c`
- `t3-sema/promote/multiply-no-widening.c`
- `t3-sema/promote/unary-minus-unsigned-char.c`
- `t3-sema/promote/unsigned-wraparound.c`
- `t3-sema/promote/ushort-promotes-to-unsigned-int.c`

### `tcc-gnu-comma` (tcc, 1 test)

816-tcc applies the GNU comma swallow

- `t1-pp/variadic/paste-comma-empty.c`

### `tcc-hash-midline` (tcc, 2 tests)

816-tcc treats a # in mid-line as a directive

- `t1-pp/lex/hash-not-first.c`
- `t1-pp/object/empty-then-hash.c`

### `tcc-hashhash-lex` (tcc, 2 tests)

816-tcc -E loses the tokens after ###

- `t1-pp/lex/max-munch-hashes.c`
- `t1-pp/paste/operators.c`

### `tcc-hashhash-stringize` (tcc, 2 tests)

816-tcc loses the ## made by pasting # and # when stringizing

- `t1-pp/paste/hash-hash-op.c`
- `t1-pp/std/c17-6.10.3.3-ex.c`

### `tcc-if-arith` (tcc, 6 tests)

816-tcc evaluates #if in its own int types, not intmax_t/uintmax_t

- `t1-pp/cond/conditional-type.c`
- `t1-pp/cond/intmax-not-int.c`
- `t1-pp/cond/negative-literal-unsigned.c`
- `t1-pp/cond/shifts.c`
- `t1-pp/cond/unary-operators.c`
- `t1-pp/cond/unsigned-conversion.c`

### `tcc-if-eager` (tcc, 1 test)

816-tcc diagnoses division by zero in unevaluated #if operands

- `t1-pp/cond/short-circuit.c`

### `tcc-if-lax` (tcc, 7 tests)

816-tcc accepts malformed #if expressions

- `t1-pp/cond/assignment.c`
- `t1-pp/cond/comma-operator.c`
- `t1-pp/cond/defined-number.c`
- `t1-pp/cond/extra-rparen.c`
- `t1-pp/cond/float-in-if.c`
- `t1-pp/cond/overflow.c`
- `t1-pp/cond/string-in-if.c`

### `tcc-implicit-function` (tcc, 1 test)

816-tcc declares it implicitly

- `t3-sema/constraint/implicit-function-declaration.c`

### `tcc-implicit-int` (tcc, 2 tests)

816-tcc accepts implicit int

- `t2-parse/decl/kr-undeclared-param.c`
- `t2-parse/errors/missing-type-c99.c`

### `tcc-lax-decl` (tcc, 18 tests)

816-tcc accepts this silently

- `t2-parse/errors/array-of-void.c`
- `t2-parse/errors/auto-at-file-scope.c`
- `t2-parse/errors/cast-to-array.c`
- `t2-parse/errors/empty-struct.c`
- `t2-parse/errors/function-body-in-declaration-list.c`
- `t2-parse/errors/keyword-as-identifier.c`
- `t2-parse/errors/member-without-type.c`
- `t2-parse/errors/register-at-file-scope.c`
- `t2-parse/errors/restrict-on-non-pointer.c`
- `t2-parse/errors/return-type-function.c`
- `t2-parse/errors/two-storage-classes.c`
- `t2-parse/errors/two-types.c`
- `t3-sema/constraint/bitfield-wider-than-int.c`
- `t3-sema/constraint/conflicting-types.c`
- `t3-sema/constraint/duplicate-member.c`
- `t3-sema/constraint/redeclare-different-linkage.c`
- `t3-sema/constraint/redefinition.c`
- `t3-sema/constraint/void-object.c`

### `tcc-lax-init` (tcc, 1 test)

816-tcc ignores excess initializers

- `t2-parse/init/excess-initializer.c`

### `tcc-lax-params` (tcc, 12 tests)

816-tcc accepts malformed macro parameter lists

- `t1-pp/function/duplicate-param.c`
- `t1-pp/function/missing-rparen-in-params.c`
- `t1-pp/function/trailing-comma-params.c`
- `t1-pp/function/unterminated-params.c`
- `t1-pp/paste/at-end.c`
- `t1-pp/paste/at-start.c`
- `t1-pp/paste/function-at-end.c`
- `t1-pp/paste/function-at-start.c`
- `t1-pp/stringize/hash-at-end.c`
- `t1-pp/stringize/hash-not-param.c`
- `t1-pp/variadic/ellipsis-not-last.c`
- `t1-pp/variadic/too-few-variadic-args.c`

### `tcc-lax-register` (tcc, 1 test)

816-tcc allows & on a register variable

- `t3-sema/constraint/address-of-register.c`

### `tcc-lax-return` (tcc, 1 test)

816-tcc accepts it

- `t3-sema/constraint/return-without-value.c`

### `tcc-lax-sizeof` (tcc, 2 tests)

816-tcc accepts sizeof of a bit-field

- `t3-sema/constraint/sizeof-bitfield.c`
- `t3-sema/constraint/sizeof-function.c`

### `tcc-lax-switch` (tcc, 3 tests)

816-tcc accepts duplicate case labels

- `t3-sema/constraint/case-duplicate-after-constant-folding.c`
- `t3-sema/constraint/case-enum-duplicate.c`
- `t3-sema/constraint/duplicate-case.c`

### `tcc-lax-types` (tcc, 7 tests)

816-tcc converts silently

- `t3-sema/constraint/argument-type.c`
- `t3-sema/constraint/function-returns-array-typedef.c`
- `t3-sema/constraint/sizeof-void.c`
- `t3-sema/constraint/struct-as-condition.c`
- `t3-sema/constraint/unary-minus-pointer.c`
- `t3-sema/constraint/void-function-value-used.c`
- `t3-sema/constraint/void-in-condition.c`

### `tcc-lex-lax` (tcc, 2 tests)

816-tcc -E passes the stray quote through

- `t1-pp/lex-errors/unterminated-char.c`
- `t1-pp/lex-errors/unterminated-string.c`

### `tcc-line-after` (tcc, 23 tests)

816-tcc reports #if/#ifdef errors on the line after the directive

- `t1-pp/cond/defined-missing-name.c`
- `t1-pp/cond/defined-missing-rparen.c`
- `t1-pp/cond/div-by-zero.c`
- `t1-pp/cond/empty-elif.c`
- `t1-pp/cond/empty-if.c`
- `t1-pp/cond/function-macro-in-if-unterminated.c`
- `t1-pp/cond/ifdef-no-name.c`
- `t1-pp/cond/ifndef-no-name.c`
- `t1-pp/cond/missing-operand.c`
- `t1-pp/cond/mod-by-zero.c`
- `t1-pp/cond/question-without-colon.c`
- `t1-pp/cond/unbalanced-parens.c`
- `t1-pp/directive/define-no-name.c`
- `t1-pp/directive/undef-no-name.c`
- `t1-pp/include/empty.c`
- `t1-pp/include/not-a-name.c`
- `t1-pp/include/unterminated-comment-in-header.c`
- `t1-pp/lex-errors/unterminated-comment.c`
- `t1-pp/line/line-directive-empty.c`
- `t2-parse/errors/goto-undefined-label.c`
- `t2-parse/errors/initializer-missing-expression.c`
- `t2-parse/errors/sizeof-no-operand.c`
- `t2-parse/errors/unterminated-block-comment-in-code.c`

### `tcc-line-lax` (tcc, 3 tests)

816-tcc accepts any #line number

- `t1-pp/line/line-directive-bad-file.c`
- `t1-pp/line/line-directive-hex.c`
- `t1-pp/line/line-directive-zero.c`

### `tcc-line-macro-hang` (tcc, 1 test)

816-tcc hangs on #line with macro operands

- `t1-pp/line/line-directive-macro.c`

### `tcc-line-octal` (tcc, 1 test)

816-tcc reads the #line number as octal

- `t1-pp/line/line-directive-decimal.c`

### `tcc-long16` (tcc, 6 tests)

816-tcc's long is 16 bits

- `t3-sema/consttype/decimal.c`
- `t3-sema/consttype/hex-octal.c`
- `t3-sema/consttype/suffixes.c`
- `t3-sema/promote/int-vs-unsigned-long.c`
- `t3-sema/promote/long-vs-unsigned-int.c`
- `t3-sema/promote/sizes.c`

### `tcc-mcpp` (tcc, 35 tests)

816-tcc fails this mcpp test: missing error at e_12_8.c:6 (directive line 6)

- `t1-pp/mcpp/e_12_8.c`
- `t1-pp/mcpp/e_14.c`
- `t1-pp/mcpp/e_14_2.c`
- `t1-pp/mcpp/e_14_3.c`
- `t1-pp/mcpp/e_14_7.c`
- `t1-pp/mcpp/e_14_9.c`
- `t1-pp/mcpp/e_15_3.c`
- `t1-pp/mcpp/e_16.c`
- `t1-pp/mcpp/e_17.c`
- `t1-pp/mcpp/e_17_5.c`
- `t1-pp/mcpp/e_18_4.c`
- `t1-pp/mcpp/e_19_3.c`
- `t1-pp/mcpp/e_23_3.c`
- `t1-pp/mcpp/e_24_6.c`
- `t1-pp/mcpp/e_29_3.c`
- `t1-pp/mcpp/e_31.c`
- `t1-pp/mcpp/e_31_3.c`
- `t1-pp/mcpp/e_32_5.c`
- `t1-pp/mcpp/e_33_2.c`
- `t1-pp/mcpp/e_7_4.c`
- `t1-pp/mcpp/e_intmax.c`
- `t1-pp/mcpp/e_pragma.c`
- `t1-pp/mcpp/e_ucn.c`
- `t1-pp/mcpp/n_13_5.c`
- `t1-pp/mcpp/n_13_7.c`
- `t1-pp/mcpp/n_2.c`
- `t1-pp/mcpp/n_21.c`
- `t1-pp/mcpp/n_26.c`
- `t1-pp/mcpp/n_27.c`
- `t1-pp/mcpp/n_3.c`
- `t1-pp/mcpp/n_4.c`
- `t1-pp/mcpp/n_7.c`
- `t1-pp/mcpp/n_9.c`
- `t1-pp/mcpp/n_pragma.c`
- `t1-pp/mcpp/n_tlimit.c`

### `tcc-memory-full` (tcc, 3 tests)

816-tcc fails with 'memory full' when an invocation's arguments continue past the end of a macro

- `t1-pp/function/invocation-spans-macro-end.c`
- `t1-pp/rescan/hide-set-intersection.c`
- `t1-pp/rescan/rescan-with-rest-of-source.c`

### `tcc-missing-endif-eof` (tcc, 2 tests)

816-tcc reports a missing #endif at the end of the file

- `t1-pp/cond/if-after-missing-endif-in-include.c`
- `t1-pp/cond/missing-endif.c`

### `tcc-no-array-static` (tcc, 2 tests)

816-tcc rejects static in array parameter declarators (C99)

- `t2-parse/decl/abstract-in-prototype.c`
- `t2-parse/decl/array-parameter-forms.c`

### `tcc-no-c11-align` (tcc, 1 test)

816-tcc has no _Alignas/_Alignof

- `t2-parse/decl/alignas-alignof.c`

### `tcc-no-c99-ws` (tcc, 1 test)

816-tcc accepts it silently

- `t1-pp/object/whitespace-after-name.c`

### `tcc-no-counter` (tcc, 3 tests)

816-tcc has no __COUNTER__

- `t1-pp/predef/counter-in-macro.c`
- `t1-pp/predef/counter-paste.c`
- `t1-pp/predef/counter.c`

### `tcc-no-digraphs` (tcc, 4 tests)

816-tcc does not know digraphs

- `t1-pp/lex/digraph-paste.c`
- `t1-pp/lex/digraph-stringize.c`
- `t1-pp/lex/digraphs.c`
- `t1-pp/stringize/digraph-hash.c`

### `tcc-no-dollar` (tcc, 1 test)

816-tcc does not accept $ in identifiers

- `t1-pp/lex/dollar-identifier.c`

### `tcc-no-for-decl` (tcc, 3 tests)

816-tcc rejects declarations in for (C99)

- `t2-parse/expr/comma.c`
- `t2-parse/stmt/all-statements.c`
- `t2-parse/stmt/for-declaration-scope.c`

### `tcc-no-generic` (tcc, 1 test)

816-tcc has no _Generic

- `t2-parse/expr/generic.c`

### `tcc-no-has-include` (tcc, 3 tests)

816-tcc has no __has_include

- `t1-pp/include/has-include-macro.c`
- `t1-pp/include/has-include-outside-if.c`
- `t1-pp/include/has-include.c`

### `tcc-no-hosted` (tcc, 1 test)

816-tcc does not define __STDC_HOSTED__

- `t1-pp/predef/stdc-hosted.c`

### `tcc-no-noreturn` (tcc, 1 test)

816-tcc has no _Noreturn

- `t2-parse/decl/noreturn-inline.c`

### `tcc-no-overflow-diag` (tcc, 2 tests)

816-tcc accepts it

- `t3-sema/consttype/enum-range.c`
- `t3-sema/consttype/overflow-diagnosed.c`

### `tcc-no-paste-diag` (tcc, 1 test)

816-tcc pastes silently

- `t1-pp/paste/invalid-token-comment.c`

### `tcc-no-pragma-E` (tcc, 3 tests)

816-tcc -E drops #pragma lines

- `t1-pp/pragma/not-expanded.c`
- `t1-pp/pragma/passthrough.c`
- `t1-pp/pragma/stdc.c`

### `tcc-no-pragma-once` (tcc, 1 test)

816-tcc ignores #pragma once

- `t1-pp/include/pragma-once.c`

### `tcc-no-pragma-op` (tcc, 10 tests)

816-tcc -E drops _Pragma

- `t1-pp/pragma/in-macro-arg.c`
- `t1-pp/pragma/operator-escapes.c`
- `t1-pp/pragma/operator-in-macro.c`
- `t1-pp/pragma/operator-mid-line.c`
- `t1-pp/pragma/operator-no-paren.c`
- `t1-pp/pragma/operator-not-string.c`
- `t1-pp/pragma/operator-unterminated.c`
- `t1-pp/pragma/operator-wide.c`
- `t1-pp/pragma/operator.c`
- `t1-pp/std/c17-6.10.9-pragma-ex.c`

### `tcc-no-redef-diag` (tcc, 4 tests)

816-tcc accepts incompatible macro redefinitions silently

- `t1-pp/object/redefine-different.c`
- `t1-pp/object/redefine-object-as-function.c`
- `t1-pp/object/redefine-whitespace-presence.c`
- `t1-pp/std/c17-6.10.3.5-ex6-invalid.c`

### `tcc-no-static-assert` (tcc, 1 test)

816-tcc has no _Static_assert

- `t2-parse/decl/static-assert.c`

### `tcc-no-trigraphs` (tcc, 1 test)

816-tcc does not replace trigraphs

- `t1-pp/lex/trigraphs.c`

### `tcc-no-u8` (tcc, 2 tests)

816-tcc has no u8 string prefix (C11)

- `t1-pp/lex/prefix-not-macro.c`
- `t1-pp/stringize/u8-string.c`

### `tcc-no-ucn` (tcc, 1 test)

816-tcc has no universal character names

- `t1-pp/lex/ucn-identifier.c`

### `tcc-no-va-opt` (tcc, 10 tests)

816-tcc has no __VA_OPT__

- `t1-pp/variadic/va-opt-basic.c`
- `t1-pp/variadic/va-opt-c23-examples.c`
- `t1-pp/variadic/va-opt-empty-expansion.c`
- `t1-pp/variadic/va-opt-named-params.c`
- `t1-pp/variadic/va-opt-nested.c`
- `t1-pp/variadic/va-opt-no-paren.c`
- `t1-pp/variadic/va-opt-not-variadic.c`
- `t1-pp/variadic/va-opt-paste.c`
- `t1-pp/variadic/va-opt-stringize.c`
- `t1-pp/variadic/va-opt-unbalanced.c`

### `tcc-rvalue-member` (tcc, 1 test)

816-tcc rejects a member of a struct rvalue (lvalue expected)

- `t3-sema/expr/member-of-rvalue-struct.c`

### `tcc-skipped-apostrophe` (tcc, 1 test)

816-tcc rejects an unmatched ' in a skipped group

- `t1-pp/lex-errors/string-in-skipped-group.c`

### `tcc-stray` (tcc, 2 tests)

816-tcc rejects stray characters

- `t1-pp/lex/stray-characters.c`
- `t1-pp/stringize/backslash-outside-literal.c`

### `tcc-stringize-space` (tcc, 3 tests)

816-tcc drops the space in "strncmp(...) == 0"

- `t1-pp/std/c17-6.10.3.5-ex4.c`
- `t1-pp/stringize/comments-become-space.c`
- `t1-pp/stringize/leading-space-from-macro.c`

### `tcc-tag-scope` (tcc, 1 test)

816-tcc rejects redeclaring a struct tag in an inner block

- `t2-parse/decl/struct-tag-scope.c`

### `tcc-unknown-directive` (tcc, 1 test)

816-tcc ignores unknown directives

- `t1-pp/directive/unknown.c`

### `tcc-unterminated-invocation` (tcc, 1 test)

816-tcc reports an unterminated invocation at EOF as too many arguments

- `t1-pp/function/unterminated-invocation.c`

### `tcc-va-args-anywhere` (tcc, 3 tests)

816-tcc accepts __VA_ARGS__ anywhere

- `t1-pp/variadic/define-va-args.c`
- `t1-pp/variadic/va-args-in-text.c`
- `t1-pp/variadic/va-args-outside.c`

### `tcc-void-arith` (tcc, 1 test)

816-tcc does void * arithmetic (GNU)

- `t3-sema/constraint/void-pointer-arithmetic.c`

### `tcc-zero-array` (tcc, 1 test)

816-tcc accepts [0]

- `t3-sema/constraint/zero-array-size.c`

### `tcc-bank-wrap` (tcc-rom, 1 test)

816-tcc's stores through p++ wrap within bank $7E

- `t5-snes/data/wram-bank-crossing.c`

### `tcc-deref-call-spill` (tcc-rom, 1 test)

816-tcc spills f into its stack slot after pushing the arguments (S-relative offset not adjusted), so the call jumps to garbage

- `t4-exec/call/function-pointer-deref-call.c`

### `tcc-fold-host-int` (tcc-rom, 1 test)

816-tcc folds (i16)(u16)65535u to 65535, not -1

- `t4-exec/arith16/const-cast-fold.c`

### `tcc-for-empty-cond` (tcc-rom, 2 tests)

816-tcc compiles for (;; step) with no condition into a jump to itself (the body never runs)

- `t4-exec/algo/state-machine.c`
- `t4-exec/control/for-empty-condition.c`

### `tcc-long16` (tcc-rom, 2 tests)

816-tcc's long is 16 bits

- `t4-exec/arith32/long-is-32.c`
- `t4-exec/stdio/printf-long.c`

### `tcc-no-llshift` (tcc-rom, 1 test)

816-tcc calls tcc__ashldi3 for variable 32-bit shifts and PVSnesLib's libtcc lacks it

- `t4-exec/arith32/shifts.c`

### `tcc-ptr-to-int32` (tcc-rom, 2 tests)

816-tcc converts a pointer to a 32-bit integer by sign-extending its low word, losing the bank

- `t5-snes/data/pointer-to-u32.c`
- `t5-snes/hw/dma-rom-to-wram.c`

### `tcc-t7` (tcc-rom, 6 tests)

816-tcc gets this generated program wrong (main returned 1); most such cases are tcc-fold-host-int

- `t7-random/corpus/seed-11.c`
- `t7-random/corpus/seed-14.c`
- `t7-random/corpus/seed-18.c`
- `t7-random/corpus/seed-35.c`
- `t7-random/corpus/seed-37.c`
- `t7-random/corpus/seed-38.c`

### `tcc-vararg-char` (tcc-rom, 1 test)

816-tcc pushes a char variadic argument as one byte

- `t4-exec/stdio/printf-char-arguments.c`

### `uncoded` (tcc-rom, 3 tests)

deliberately calls abort()

- `t4-exec/smoke/xfail-harness-detects-abort.c`
- `t4-exec/smoke/xfail-harness-detects-nonzero.c`
- `t4-exec/stdio/output-mismatch-detected.c`

