// loomcc-do: run
// loomcc-int: agnostic
// 8.8 fixed point, the way Loom's movement code uses it.
#include "loomcc-test.h"
typedef i16 fx8;
static fx8 fx_mul(fx8 a, fx8 b) { return (fx8)(((i32)a * b) >> 8); }
int main(void) {
  fx8 one = 256, half = 128, three = 3 * 256;
  CHECK(fx_mul(one, one) == one);
  CHECK(fx_mul(half, half) == 64);
  CHECK(fx_mul(three, -half) == -384);
  CHECK(fx_mul(-one, -one) == one);
  return 0;
}
