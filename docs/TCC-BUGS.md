# 816-tcc miscompiles

Code-generation bugs in 816-tcc 0.9.25 (PVSnesLib devkitsnes, with
816-opt 2.0.0), found while building this suite's references. Each silently
produces wrong code, and each appears in ordinary C. Loom's runtime and
generator do not use these patterns, but user hooks could. Each reproducer is
a complete harness test: run it with

```sh
./run-tests --refs tcc-rom --refs-only <file>
```

It fails under 816-tcc and passes under host clang, the 16-bit-int host
reference and loomcc.

## 1. `for` with a step but no condition never runs its body

Test: `tests/t4-exec/control/for-empty-condition.c`

```c
int n = 0;
for (;; n++) { if (n == 3) break; }   /* hangs */
```

816-tcc emits the loop head as a jump to itself, placed before the body:

```
__local_0:
__local_1:
  jmp.w __local_0      ; 816-opt turns it into `bra __local_0`
__local_4:             ; the step (n++) and the body follow, unreachable
```

The program spins forever. `for (;;)` without a step, `while (1)` and
`for (i = 0; i < n; i++)` compile correctly. Workaround: write the condition
(`for (; 1; n++)`) or use `while (1) { ...; n++; }`.

## 2. `(*fp)(args)` corrupts an argument and jumps to garbage

Test: `tests/t4-exec/call/function-pointer-deref-call.c`

```c
static short sub(short a, short b) { return a - b; }
int main(void) {
  short (*f)(short, short) = sub;
  if ((*f)(10, 3) != 7) abort();      /* crashes or hangs */
  return 0;
}
```

816-tcc pushes the arguments, then reloads `f` from its stack slot and spills
it again. It addresses the slot `S`-relative, using the offset from before
the pushes:

```
  pea.w 3
  pea.w 10                                  ; S has moved down 4 bytes
  lda.b tcc__r0
  sta -4 + __main_locals + 1,s              ; overwrites the pushed 10/3, not f
  sta.b tcc__r10
  lda -2 + __main_locals + 1,s              ; reads a pushed word as f's bank
  sta.b tcc__r10h
  jsr.l tcc__jsl_r10                        ; jumps into the wrong bank
```

A plain `f(10, 3)` compiles correctly. Workaround: never write `(*fp)(...)`.
Call through the pointer directly.

## 3. Constant folding does unsigned int arithmetic in 32 bits

Tests: `tests/t4-exec/arith16/const-cast-fold.c`,
`tests/t3-sema/promote/*.c` (declared `tcc-fold-host-int`)

```c
volatile unsigned short m = 65535u;
if ((short)m != -1) abort();                       /* run time: correct */
if ((short)(unsigned short)65535u != -1) abort();  /* folded: 816-tcc gets 65535 */
```

`int` and `unsigned int` are 16 bits on this target, and 816-tcc's generated
code agrees. Its constant folder, though, computes `unsigned int` arithmetic
(and some casts) in the host's 32 bits, so the same expression gives
different answers depending on whether its operands are constants:

| expression | correct (16-bit int) | 816-tcc folds to |
|---|---|---|
| `-1 + 0u == 65535u` | 1 | 0 (it computes 0xffffffff) |
| `-1 / 2u` | 32767 | 0x7fffffff, truncated to 65535 |
| `(1 ? -1 : 0u) == 65535u` | 1 | 0 |
| `(short)(unsigned short)65535u` | -1 | 65535 |
| `(unsigned short)-1 == 0xffffu` | 1 | 0 (the cast does not truncate) |

Reproduce the folded values directly with the layout probe:

```sh
echo 'int dummy;' > /tmp/e.c
printf '%s\n' '-1 + 0u == 65535u' '-1 / 2u' | scripts/tcc-layout.py /tmp/e.c -
```

This affects table sizes and `#define`d constants built from unsigned
arithmetic. Workaround: keep unsigned constant expressions within 16 bits
explicitly (`(unsigned)(-1)` in place of `-1` mixed with `u` operands), or
compute them at run time.

## 4. `sizeof` of an array plus an offset is the array's size

Test: `tests/t3-sema/expr/array-decay-in-sizeof-and-ops.c`

```c
static short a[10];
sizeof(a + 0)     /* must be sizeof(short *) == 4; 816-tcc says 20 */
```

`a + 0` is a pointer (the array decays), but 816-tcc keeps the array type
for the operand of `sizeof`. Only code that takes `sizeof` of such an
expression is affected (rare; `sizeof(&a[0])` is correct).

## 5. `char` arguments to variadic functions are not promoted

Test: `tests/t4-exec/stdio/printf-char-arguments.c`

```c
unsigned char a = 1, b = 2;
printf("%u %u %u\n", a, b, 3);    /* prints "513 3 0" */
```

C promotes a `char` argument in the variable part of a call to `int`
(6.5.2.2p7). 816-tcc pushes it as the single byte it pushes for a
prototyped `u8` parameter, so the callee's `va_arg(ap, int)` reads two
arguments' bytes as one and everything after is shifted. Any variadic
function (a logging helper, a `printf` port) is affected. Workaround: cast
`char`-typed arguments to `unsigned` or `int` at the call.

## 6. Converting a pointer to a 32-bit integer loses the bank

Tests: `tests/t5-snes/data/pointer-to-u32.c`, `tests/t5-snes/hw/dma-rom-to-wram.c`

```c
static const unsigned char table[4] = { 1, 2, 3, 4 };
unsigned long long a = (unsigned long long)table;   /* 816-tcc's 32-bit type */
/* a >> 16 is 0xffff or 0: the low word, sign-extended */
```

816-tcc converts the 24-bit pointer by taking its low word and sign-extending
it, so the bank is lost. Code that splits an address into DMA source
registers (`$4302-$4304`) from a pointer gets the wrong bank. The pointer
itself is right: dereferencing it works. Workaround: take the bank with the
assembler (`#:label`) or read the pointer's bytes through a union.

## Also seen (not miscompiles)

- `#if` arithmetic is not done in intmax_t (`#if 65535 + 1 == 65536` is
  false), and a self-referencing macro can be expanded again. See
  docs/DIVERGENCES.md, `tcc-if-arith` and `tcc-blue-paint`.
- Variable 32-bit (`long long`) shifts need `tcc__ashldi3`, which libtcc
  lacks: a link error, not silent.
- `816-tcc -E` always ends with SIGABRT after writing its output.
- A function with more than about 255 bytes of locals and temporaries (816-tcc
  gives every comparison result its own stack slot) produces
  `sta n,s` with n > 255, which wla-65816 rejects (`Out of 8-bit range`): a
  build error, not a miscompile. Seen with a `main` holding ~200 `if`
  comparisons; the generated tests split their checks into functions.
