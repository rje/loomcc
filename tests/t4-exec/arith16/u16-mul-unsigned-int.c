// loomcc-do: run
// loomcc-int: 16
// With 16-bit int, u16 * u16 is an unsigned int multiply that wraps
// (on a 32-bit-int host it would be a signed int overflow).
#include "loomcc-test.h"
static u16 umul(u16 a, u16 b) { return a * b; }
int main(void) {
  volatile u16 a = 65535u, b = 300;
  CHECK(a * a == 1);
  CHECK(b * b == 24464u);
  CHECK(umul(256, 256) == 0);
  CHECK(umul(65535u, 2) == 65534u);
  return 0;
}
