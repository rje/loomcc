// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
struct P { i16 x, y; };
static i16 sum(struct P p) { p.x += 100; return p.x + p.y; }
int main(void) {
  struct P a;
  a.x = 3; a.y = 4;
  CHECK(sum(a) == 107);
  CHECK(a.x == 3);
  return 0;
}
