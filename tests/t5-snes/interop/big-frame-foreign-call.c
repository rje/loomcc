// loomcc-do: run
// loomcc-int: agnostic
// A function with a 6000-byte local array that calls foreign (816-tcc)
// code: printf in the ROM harness. Whatever the compiler saves around the
// call must fit the SNES hardware stack in bank 0, and the code that saves
// it must fit a ROM bank (found through tcc tests2 130_large_argument).
// loomcc-ref-diverges: tcc-rom tcc-big-frame 816-tcc cannot address a frame over 255 bytes (wla-65816 rejects the unit)
#include "loomcc-test.h"
int printf(const char *, ...);
static u16 fill(u16 seed) {
  u8 buf[6000];
  u16 i, sum = 0;
  for (i = 0; i < sizeof buf; i++) buf[i] = (u8)(i + seed);
  printf("%u\n", (unsigned)buf[5999]);
  for (i = 0; i < sizeof buf; i += 7) sum = (u16)(sum + buf[i]);
  return sum;
}
u16 (*volatile fp)(u16) = fill;
int main(void) {
  u16 s = fp(3);
  printf("%u\n", (unsigned)s);
  return 0;
}
// loomcc-expect-stdout: 114
// loomcc-expect-stdout: 42769
