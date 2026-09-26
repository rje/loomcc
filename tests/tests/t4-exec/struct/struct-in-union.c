// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
struct Pair { u8 lo, hi; };
union Word { struct Pair p; u8 raw[2]; };
int main(void) {
  union Word w;
  w.p.lo = 0x34; w.p.hi = 0x12;
  CHECK(w.raw[0] == 0x34 && w.raw[1] == 0x12);
  return 0;
}
