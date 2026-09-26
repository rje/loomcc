// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  volatile u32 a = 0x0000ffff, b = 1, c = 0xffffffff;
  CHECK(a + b == 0x00010000);
  CHECK((u32)(c + b) == 0);
  CHECK((u32)(b - a) == 0xffff0002);
  volatile i32 x = -100000, y = 70000;
  CHECK(x + y == -30000);
  CHECK(y - x == 170000);
  CHECK(-x == 100000);
  return 0;
}
