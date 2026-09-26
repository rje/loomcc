// loomcc-do: run
// loomcc-int: agnostic
// Compound assignment evaluates its left operand once.
#include "loomcc-test.h"
int main(void) {
  i16 a[5] = { 0, 0, 0, 0, 0 };
  int i = 0;
  a[i++] += 5;
  CHECK(i == 1 && a[0] == 5 && a[1] == 0);
  a[i++] *= 3;
  CHECK(i == 2 && a[1] == 0);
  a[++i] -= 7;
  CHECK(i == 3 && a[3] == -7);
  return 0;
}
