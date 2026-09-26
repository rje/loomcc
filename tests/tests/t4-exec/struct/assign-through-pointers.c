// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
typedef struct { i16 x, y; u8 flags[3]; } P;
int main(void) {
  P a, b, *pa = &a, *pb = &b;
  a.x = 1; a.y = -2; a.flags[0] = 1; a.flags[1] = 2; a.flags[2] = 3;
  *pb = *pa;
  pa->flags[1] = 20;
  CHECK(b.x == 1 && b.y == -2 && b.flags[1] == 2 && b.flags[2] == 3);
  *pa = *pb;
  CHECK(a.flags[1] == 2);
  return 0;
}
