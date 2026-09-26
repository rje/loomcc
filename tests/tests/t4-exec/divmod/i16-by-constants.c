// loomcc-do: run
// loomcc-int: agnostic
// Signed division truncates toward zero; a shift is not a signed division.
#include "loomcc-test.h"
int main(void) {
  volatile i16 a = -7, b = 7, c = -32768;
  CHECK(a / 2 == -3);
  CHECK(a % 2 == -1);
  CHECK(b / -2 == -3);
  CHECK(b % -2 == 1);
  CHECK(a / 4 == -1);
  CHECK(a % 4 == -3);
  CHECK(a / 8 == 0);
  CHECK(a % 8 == -7);
  CHECK(c / 2 == -16384);
  CHECK(c / 256 == -128);
  CHECK(c % 256 == 0);
  CHECK(a / 3 == -2 && a % 3 == -1);
  CHECK(a / 10 == 0 && a % 10 == -7);
  return 0;
}
