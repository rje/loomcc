// loomcc-do: run
// loomcc-int: agnostic
// Multiplications by constants: shifts and adds must match real multiplies.
#include "loomcc-test.h"
int main(void) {
  volatile i16 x = 123, n = -45;
  CHECK(x * 0 == 0);
  CHECK(x * 1 == 123);
  CHECK(x * 2 == 246);
  CHECK(x * 3 == 369);
  CHECK(x * 5 == 615);
  CHECK(x * 7 == 861);
  CHECK(x * 10 == 1230);
  CHECK(x * 16 == 1968);
  CHECK(x * 24 == 2952);
  CHECK(x * 255 == 31365);
  CHECK(x * -1 == -123);
  CHECK(n * 3 == -135);
  CHECK(n * 64 == -2880);
  CHECK(n * -7 == 315);
  return 0;
}
