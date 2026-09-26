// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static i16 sgn(i16 x) { return x < 0 ? -1 : x > 0 ? 1 : 0; }
static i16 max(i16 a, i16 b) { return a > b ? a : b; }
int main(void) {
  CHECK(sgn(-5) == -1 && sgn(0) == 0 && sgn(9) == 1);
  CHECK(max(-3, -4) == -3);
  CHECK(max(32767, -32768) == 32767);
  return 0;
}
