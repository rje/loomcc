// loomcc-do: run
// loomcc-int: 16
// With 16-bit int, u16 promotes to unsigned int: a + 1 wraps before the compare.
#include "loomcc-test.h"
int main(void) {
  volatile u16 a = 65535;
  CHECK(a + 1 == 0);
  CHECK(a + 1 < a);
  volatile u16 b = 1, c = 2;
  CHECK(b - c > 0);
  CHECK(b - c == 65535u);
  return 0;
}
