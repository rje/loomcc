// loomcc-do: run
// loomcc-int: agnostic
// loomcc-note: F38, found by Csmith seed 4103 (C-Reduce, then by hand)
// Branching on the result of a shift by a variable count: the shift is a
// loop ending in `dey`, so the zero flag reflects the count, not the
// result. loomcc took `if (1 << n)` as false and skipped the loop body.
#include "loomcc-test.h"
static i16 g;
static u8 hits;
static u16 shl(u16 a, u16 b) { return b >= 16 ? a : (u16)(a << b); }
static u16 shr(u16 a, u16 b) { return b >= 16 ? 0 : (u16)(a >> b); }
int main(void) {
  u8 i;
  g = 6;
  if (shl(1, 0 < g)) hits++;
  if (shl(1, g)) hits++;
  if (shr(0x8000, g)) hits++;
  for (i = 1; i < 4; i++)
    if (shl(i, i)) hits++;
  if (shr(1, g)) hits += 100;
  CHECK(hits == 6);
  return 0;
}
