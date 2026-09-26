// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  volatile u8 a = 255;
  u8 b = a;
  b++;
  CHECK(b == 0);
  b--;
  CHECK(b == 255);
  b = (u8)(a + 2);
  CHECK(b == 1);
  b = (u8)(a * 2);
  CHECK(b == 254);
  b = (u8)(a << 4);
  CHECK(b == 0xf0);
  return 0;
}
