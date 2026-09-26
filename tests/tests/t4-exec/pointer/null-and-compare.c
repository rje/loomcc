// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static i16 g;
int main(void) {
  i16 *p = 0;
  CHECK(p == 0);
  CHECK(!p);
  p = &g;
  CHECK(p != 0 && p);
  return 0;
}
