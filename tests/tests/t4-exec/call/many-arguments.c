// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static i32 f(i8 a, i16 b, i32 c, u8 d, u16 e, i16 *p, u8 g, i32 h) {
  return a + b + c + d + e + *p + g + h;
}
int main(void) {
  i16 v = 7;
  CHECK(f(-1, -2, 100000, 255, 65535u, &v, 1, -100000) == -1 - 2 + 255 + 65535 + 7 + 1);
  return 0;
}
