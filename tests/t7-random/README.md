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
tests/t7-random/run.py --seeds 1-500 --narrow -j 3            # 8/16-bit only
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

## Csmith

`run.py --csmith` generates with Csmith 2.3.0 (`brew install csmith`):

```sh
tests/t7-random/run.py --csmith --seeds 1-200 -j 3
```

Flags: `--no-argc --no-longlong --no-math64 --no-bitfields --no-packed-struct
--no-float --max-funcs 4 --max-block-size 3`. Programs include
`tests/t7-random/csmith/csmith.h` (found first on the include path), which
sets 16-bit-int limits so that Csmith's own safe-math wrappers
(`safe_math_16.h`, its safe_math.h without the 64-bit and floating-point
parts) guard every 16-bit operation, and replaces the CRC32 checksum with a
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
program reduced to a few lines around one `^`.
