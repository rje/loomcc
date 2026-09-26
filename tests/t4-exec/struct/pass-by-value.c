// loomcc-do: run
// loomcc-ref-diverges: host16 [lli-byval] the LLVM interpreter ignores byval: the callee writes the caller's struct
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
