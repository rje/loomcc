// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  volatile u16 a = 60000, b = 7;
  CHECK(a / 1 == 60000);
  CHECK(a / 2 == 30000);
  CHECK(a / 3 == 20000);
  CHECK(a / 7 == 8571);
  CHECK(a % 7 == 3);
  CHECK(a / 10 == 6000);
  CHECK(a % 10 == 0);
  CHECK(a / 16 == 3750);
  CHECK(a % 16 == 0);
  CHECK(a / 256 == 234);
  CHECK(a % 256 == 96);
  CHECK(a / 1000 == 60);
  CHECK(a / 60001u == 0);
  CHECK(b / 8 == 0 && b % 8 == 7);
  return 0;
}
