// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static u32 fact(u16 n) { return n <= 1 ? 1 : n * fact((u16)(n - 1)); }
int main(void) {
  CHECK(fact(0) == 1);
  CHECK(fact(5) == 120);
  CHECK(fact(12) == 479001600u);
  return 0;
}
