// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static int popcount(u16 x) { int n = 0; while (x) { x &= (u16)(x - 1); n++; } return n; }
static u16 reverse(u16 x) { u16 r = 0; int i; for (i = 0; i < 16; i++) { r = (u16)((r << 1) | (x & 1)); x >>= 1; } return r; }
static int lowest_set(u16 x) { int i = 0; if (!x) return -1; while (!(x & 1)) { x >>= 1; i++; } return i; }
int main(void) {
  CHECK(popcount(0) == 0 && popcount(0xffff) == 16 && popcount(0x8001) == 2 && popcount(0x5555) == 8);
  CHECK(reverse(1) == 0x8000 && reverse(0x1234) == 0x2c48);
  CHECK(lowest_set(0x8000) == 15 && lowest_set(0x0006) == 1 && lowest_set(0) == -1);
  return 0;
}
