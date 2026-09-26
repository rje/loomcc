// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  volatile u8 a = 250, b = 7;
  u8 q = a / b, r = a % b;
  CHECK(q == 35 && r == 5);
  CHECK(a / 16 == 15 && a % 16 == 10);
  volatile i8 c = -100, d = 9;
  CHECK(c / d == -11 && c % d == -1);
  return 0;
}
