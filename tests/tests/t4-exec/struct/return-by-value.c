// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
struct P { i16 x, y; u8 z[3]; };
static struct P make(i16 x, i16 y) { struct P p; p.x = x; p.y = y; p.z[0] = 1; p.z[1] = 2; p.z[2] = 3; return p; }
int main(void) {
  struct P p = make(10, -20);
  CHECK(p.x == 10 && p.y == -20 && p.z[2] == 3);
  CHECK(make(1, 2).y == 2);
  return 0;
}
