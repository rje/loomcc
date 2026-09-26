// loomcc-do: run
// loomcc-int: agnostic
// A 16-bit sum kept as two u8 halves with a manual carry.
#include "loomcc-test.h"
int main(void) {
  static const u8 bytes[6] = { 200, 100, 255, 1, 128, 128 };
  u8 lo = 0, hi = 0, i;
  for (i = 0; i < 6; i++) {
    u8 old = lo;
    lo = (u8)(lo + bytes[i]);
    if (lo < old) hi++;
  }
  CHECK(lo == (u8)812 && hi == 3);
  CHECK((u16)(hi << 8 | lo) == 812);
  return 0;
}
