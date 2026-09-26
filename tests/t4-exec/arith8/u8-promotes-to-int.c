// loomcc-do: run
// loomcc-int: agnostic
// u8 arithmetic happens in int: 200 + 100 is 300 before any store.
#include "loomcc-test.h"
int main(void) {
  volatile u8 a = 200, b = 100;
  i16 s = a + b;
  CHECK(s == 300);
  CHECK(a + b > 255);
  CHECK(a - b == 100);
  CHECK(b - a == -100);
  CHECK(a * b == 20000);
  return 0;
}
