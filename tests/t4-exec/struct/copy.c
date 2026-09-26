// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
struct P { i16 x, y; u8 tag; };
int main(void) {
  struct P a, b;
  a.x = 1; a.y = -2; a.tag = 200;
  b = a;
  CHECK(b.x == 1 && b.y == -2 && b.tag == 200);
  a.x = 5;
  CHECK(b.x == 1);
  return 0;
}
