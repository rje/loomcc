# T7: randomised differential testing

`gen.py` is a small Csmith-style generator written for this suite: it
emits self-checking programs in the subset loomcc's back end supports first
(8/16/32-bit integers, arrays, calls, loops, `if`, `switch`), free of
undefined behaviour at any `int` width (signed arithmetic goes through the
unsigned type of the same width, 16-bit products and shifts through `1u *`,
divisors and shift counts are guarded). The program folds its globals into
a 16-bit checksum.

`run.py` drives a campaign:

```sh
tests/t7-random/run.py --seeds 1-500 --narrow -j 2            # 8/16-bit only
tests/t7-random/run.py --seeds 1-200 --stmts 20                # with 32-bit types
tests/t7-random/run.py --seeds 1-50 --dir tests/t7-random/corpus --narrow   # keep as tests
```

For each seed the checksum is computed by **host16** (clang's msp430 front
end: 16-bit `int`, 32-bit `long`, run by `lli`); the program becomes a
runner test with `-DEXPECTED=<checksum>`, and the runner runs loomcc's `ir`
and `rom` modes and 816-tcc's ROM. A loomcc failure other than "not
supported yet" is reported as a LIKELY LOOMCC BUG with 816-tcc's verdict
beside it. 816-tcc is a weak oracle here: its constant folder computes
unsigned int arithmetic in 32 bits (`tcc-fold-host-int`), and these
programs are full of constant subexpressions; it also rejects some 32-bit
(`long long`) expressions with `error: 42 (Deep Thought)`.

`corpus/` holds 40 generated `--narrow` programs as permanent regression
tests (their 816-tcc failures are declared `tcc-t7`).

## Shapes (large structs, deep call chains)

`--shapes` adds what F29 and F30 are about: structs of up to ~500 bytes
(scalar fields and an array member) as globals, passed and returned by
value and read through pointers; a chain of 6-24 distinct functions (no
recursion), each copying a struct and filling a local array of up to 1 KiB,
passing the struct down by value; a function of 8-16 parameters; and a
call into foreign code (`printf("%s", "")`, 816-tcc code on the ROM) in each.
816-tcc cannot build frames over 255 bytes, so host16 is the only reference.

```sh
tests/t7-random/run.py --shapes --seeds 1-60 -j 2                        # everything
tests/t7-random/run.py --shapes --no-foreign --small --seeds 1-60 -j 2   # clear of F29 and F30
```

`--no-foreign` compiles the programs with `-DT7_NO_FOREIGN` (no calls into
816-tcc code, so F30's frame save never happens); `--small` keeps every
struct under about 110 bytes (clear of F29's 8-bit stack offsets). The
campaign reports a link failure for code too big for a bank as known
(F28/F30) rather than as a suspect.

## Csmith

`run.py --csmith` generates with Csmith 2.3.0 (`brew install csmith`):

```sh
tests/t7-random/run.py --csmith --seeds 1-200 -j 2
```

Flags: `--no-argc --no-longlong --no-math64 --no-bitfields --no-packed-struct
--no-float --max-funcs 4 --max-block-size 3`. Campaigns so far (seeds 11-110 and 211-310, 176 programs that host16 could
run; the others crash or time out in lli): no loomcc disagreement. They
flagged three host16 errors, all fixed in `harness/host16/fixup.py`: lli
ignored `byval` for a global struct passed by value (seed 73;
`t4-exec/struct/global-passed-by-value.c` pins it), pointer arrays
initialised by memcpy were half-copied because clang sized the copy for
2-byte pointers (seed 106), and a union initialised from its first member's
smaller constant was over-copied, reading garbage (seed 278: copy lengths now
come from a model of the widened layout, the smaller of source and
destination). Seed 239 has a `func_1` that loomcc compiles to 34,570 bytes,
more than a 32 KiB LoROM bank, so it cannot link (816-tcc cannot even
assemble it). Programs include
`tests/t7-random/csmith/csmith.h` (found first on the include path), which
sets 16-bit-int limits so that Csmith's own safe-math wrappers
(`safe_math_16.h`, its safe_math.h without the 64-bit and floating-point
parts, and with the 8/16-bit shift wrappers rejecting counts of 16 and up
where Csmith hard-codes 32) guard every 16-bit operation, and replaces the CRC32 checksum with a
16-bit fold. The reference is host16; 816-tcc is not used (its `long`,
which Csmith's 32-bit constants need, is 16 bits).

## Reduction

`reduce.py FAILING.c --mode ir|rom` reduces a disagreeing program with
C-Reduce (`brew install creduce`; cvise has no Homebrew formula). The
interestingness test keeps a candidate only if clang (16-bit int) accepts it
with the usual UB-signalling warnings made errors, it keeps the checksum
scaffolding, host16 runs it to completion, and loomcc's chosen mode still
disagrees with host16's checksum. The result is written with the runner
header and `EXPECTED` from host16, ready to become a T4 test.

It was checked with a deliberately broken loomcc (a wrapper that makes
`--run-ir` fail on any program containing `^`): a 330-line generated
program reduced to 18 lines (one `^`, plus the checksum scaffolding) in
about ten minutes.

## Recursion (`--recursion`)

`gen.py --recursion` adds recursive functions of bounded depth: a depth
parameter shrinks by one each level, at most 12 levels. There is one
self-recursive function, a mutually recursive pair and a tail-recursive
one. Each keeps an array local, passes a pointer to it down the recursion
and writes through its caller's pointer, so the program is correct only if
every activation has its own locals.

Campaigns of 2026-09-26 (loomcc 70e5955 to 0a86f99):

| campaign | programs | loomcc results | finding |
|---|---:|---|---|
| `--narrow --recursion --seeds 1001-1300` | 299 | 597 of 598 pass | seed 1009 was a generator bug: a call inside a guarded division could change the divisor between the guard and the division. Calls are no longer generated there. |
| `--recursion --seeds 2001-2200` (32-bit) | 200 | 400 of 400 pass | none |
| `--shapes --small --no-foreign --recursion --seeds 3001-3100` | 98 | 294 of 294 pass | none |
| `--csmith --seeds 4001-4200` | ~180 | all pass but 4103, and 3 that are too big for a bank (F28) | seed 4103: F38 (a branch on a variable shift), fixed |

816-tcc, the second reference, fails most 32-bit programs (F-tcc notes above).

Round 2 (loomcc 4ddb3cf):

| campaign | programs | loomcc results | finding |
|---|---:|---|---|
| `--csmith --seeds 4201-4600` | ~350 | 2 suspects, 5 too big for a bank (F28) | both suspects were host16 errors, now fixed or excluded (below) |
| `--narrow --recursion --stmts 16 --seeds 5001-5400` | 394 | all pass | none |
| `--recursion --seeds 6001-6200` (32-bit) | 198 | all pass | none |

Two more host16 limits:
- **Seed 4438.** A struct passed by value from an element of a global array
  is a `getelementptr` constant expression. `fixup.py` copied only
  arguments that are plain names, so the callee's writes reached the
  global. It now copies constant expressions too.
- **Seed 4590.** host16 widens pointers to 64 bits for lli. That changes
  the size of a union with a pointer member, while clang types an
  initialised array of such unions by its first member, so indexing reads
  past the elements. `run.py --csmith` now skips programs with such a
  union.

`reduce.py` accepts clang's distinct-pointer-type comparisons, which Csmith
writes, and rejects tentative definitions of incomplete arrays, which
C-Reduce introduces.

