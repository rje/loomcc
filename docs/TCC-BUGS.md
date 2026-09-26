# 816-tcc miscompiles

Three code-generation bugs in 816-tcc 0.9.25 (PVSnesLib devkitsnes, with
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

Reproduce the folded values directly with the layout probe:

```sh
echo 'int dummy;' > /tmp/e.c
printf '%s\n' '-1 + 0u == 65535u' '-1 / 2u' | scripts/tcc-layout.py /tmp/e.c -
```

This affects table sizes and `#define`d constants built from unsigned
arithmetic. Workaround: keep unsigned constant expressions within 16 bits
explicitly (`(unsigned)(-1)` in place of `-1` mixed with `u` operands), or
compute them at run time.

## Also seen (not miscompiles)

- `#if` arithmetic is not done in intmax_t (`#if 65535 + 1 == 65536` is
  false), and a self-referencing macro can be expanded again. See
  docs/DIVERGENCES.md, `tcc-if-arith` and `tcc-blue-paint`.
- Variable 32-bit (`long long`) shifts need `tcc__ashldi3`, which libtcc
  lacks: a link error, not silent.
- `816-tcc -E` always ends with SIGABRT after writing its output.
