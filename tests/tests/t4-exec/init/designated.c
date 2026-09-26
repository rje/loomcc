// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
struct P { i16 x, y, z; };
static struct P p = { .z = 3, .x = 1 };
static i16 a[6] = { [4] = 40, [1] = 10, 11 };
int main(void) {
  struct P q = { .y = -2 };
  CHECK(p.x == 1 && p.y == 0 && p.z == 3);
  CHECK(q.x == 0 && q.y == -2 && q.z == 0);
  CHECK(a[0] == 0 && a[1] == 10 && a[2] == 11 && a[3] == 0 && a[4] == 40 && a[5] == 0);
  return 0;
}
