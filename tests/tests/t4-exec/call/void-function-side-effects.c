// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static i16 g;
static void bump(i16 by) { g += by; }
int main(void) {
  g = 0;
  bump(3); bump(-1); bump(10);
  CHECK(g == 12);
  return 0;
}
