// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static i16 a = 1, b = 2, c = 3;
static i16 *ptrs[3] = { &a, &b, &c };
int main(void) {
  int i;
  i16 s = 0;
  for (i = 0; i < 3; i++) s += *ptrs[i];
  CHECK(s == 6);
  *ptrs[1] = 20;
  CHECK(b == 20);
  return 0;
}
