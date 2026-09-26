// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  i16 a = 5, b;
  b = a++;
  CHECK(a == 6 && b == 5);
  b = ++a;
  CHECK(a == 7 && b == 7);
  b = a--;
  CHECK(a == 6 && b == 7);
  b = --a;
  CHECK(a == 5 && b == 5);
  a += 10; CHECK(a == 15);
  a -= 20; CHECK(a == -5);
  a *= -3; CHECK(a == 15);
  a /= 4; CHECK(a == 3);
  a %= 2; CHECK(a == 1);
  a <<= 4; CHECK(a == 16);
  a >>= 2; CHECK(a == 4);
  a |= 3; CHECK(a == 7);
  a &= 5; CHECK(a == 5);
  a ^= 1; CHECK(a == 4);
  return 0;
}
