// loomcc-do: run
// loomcc-int: agnostic
// for with an increment but no condition: the loop ends only by break.
// loomcc-ref-diverges: tcc-rom [tcc-for-empty-cond] 816-tcc compiles for (;; step) with no condition into a jump to itself (the body never runs)
#include "loomcc-test.h"
int main(void) {
  int n = 0, i;
  for (;; n++) { if (n == 3) break; }
  CHECK(n == 3);
  for (i = 10;; i -= 2) if (i < 0) break;
  CHECK(i == -2);
  return 0;
}
