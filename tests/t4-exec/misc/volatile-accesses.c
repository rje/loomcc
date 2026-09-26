// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static volatile u16 reg;
int main(void) {
  u16 a, b;
  reg = 1;
  reg = 2;
  a = reg;
  b = reg;
  CHECK(a == 2 && b == 2);
  reg += 3;
  CHECK(reg == 5);
  return 0;
}
