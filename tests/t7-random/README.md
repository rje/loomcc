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

## Csmith, YARPGen and reduction

Neither Csmith nor YARPGen is installed on this machine. When they are:

- Csmith: `csmith --no-float --no-longlong --no-math64 --no-bitfields
  --no-packed-struct --no-volatile-pointers --max-funcs 4 --no-argc
  --max-struct-fields 4` and a `csmith.h` shim that maps `platform_generic`
  types to loomcc's (int8..int32; `uint64_t` must not appear) and
  `transparent_crc` to the same 16-bit fold as gen.py; the checksum is
  computed by host16 exactly as above. Csmith's safe-math wrappers already
  avoid UB, but assume 32-bit `int` in places, so programs that fail under
  host16 but pass natively are discarded.
- YARPGen v1 (C mode) with `--std=c99 -b 16` style limits; the same host16
  oracle.

Reduction: cvise (`brew install cvise`) or C-Reduce with an interestingness
test that (1) compiles with clang -fsanitize=undefined natively without a
trap, (2) still gives host16's checksum under host16 and the 816-tcc ROM,
and (3) still makes loomcc's `ir` or `rom` disagree. A reduced case becomes
a T4 test named after the seed (`t4-exec/random/seed-<n>.c`).
