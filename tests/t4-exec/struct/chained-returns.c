// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
struct V { i16 x, y; };
static struct V add(struct V a, struct V b) { struct V r; r.x = (i16)(a.x + b.x); r.y = (i16)(a.y + b.y); return r; }
static struct V v(i16 x, i16 y) { struct V r; r.x = x; r.y = y; return r; }
int main(void) {
  struct V r = add(add(v(1, 2), v(3, 4)), add(v(-10, 5), v(0, 0)));
  CHECK(r.x == -6 && r.y == 11);
  CHECK(add(v(1, 1), v(2, 2)).x == 3);
  return 0;
}
