// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static void set(i16 **pp, i16 *target) { *pp = target; }
int main(void) {
  i16 x = 1, y = 2;
  i16 *p = &x;
  i16 **pp = &p;
  **pp = 10;
  CHECK(x == 10);
  set(pp, &y);
  CHECK(p == &y && *p == 2);
  return 0;
}
