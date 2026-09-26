// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static int lt(i32 a, i32 b) { return a < b; }
int main(void) {
  static const i32 v[] = { -2147483647 - 1, -65536, -65535, -1, 0, 1, 65535, 65536, 2147483647 };
  unsigned i, j;
  for (i = 0; i < 9; i++)
    for (j = 0; j < 9; j++)
      CHECK(lt(v[i], v[j]) == (i < j));
  return 0;
}
