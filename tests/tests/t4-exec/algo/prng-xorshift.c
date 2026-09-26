// loomcc-do: run
// loomcc-int: agnostic
// A 16-bit xorshift generator (7, 9, 8), the kind games use.
#include "loomcc-test.h"
static u16 state = 1;
static u16 next(void) { state ^= (u16)(state << 7); state ^= (u16)(state >> 9); state ^= (u16)(state << 8); return state; }
int main(void) {
  u16 x = 0;
  int i;
  for (i = 0; i < 1000; i++) x = next();
  CHECK(x != 0);
  state = 1;
  CHECK(next() == 0x8181);
  return 0;
}
