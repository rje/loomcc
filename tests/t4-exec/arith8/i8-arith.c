// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  volatile i8 a = 100, b = 50, c = -7;
  i8 r;
  r = (i8)(a + b);          /* 150 converts to -106 (implementation-defined, two's complement everywhere) */
  CHECK(r == -106);
  CHECK(a / c == -14);
  CHECK(a % c == 2);
  CHECK(c / 2 == -3);
  CHECK(c % 2 == -1);
  CHECK(-c == 7);
  return 0;
}
