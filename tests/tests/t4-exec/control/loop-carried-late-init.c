// loomcc-do: run
// loomcc-int: agnostic
// A variable first assigned inside a loop and read, on later iterations
// only, under a flag: its value must carry from one iteration to the next.
// (Same shape as GCC's PR 53465 regression test, written independently.)
#include "loomcc-test.h"
static const i16 data[] = { 1, 2, 5, 9 };
static int increasing(const i16 *x, int n) {
  int i, seen = 0;
  i16 prev;
  for (i = 0; i < n; i++) {
    i16 cur = x[i];
    if (seen && cur <= prev) return 0;
    prev = cur;
    seen = 1;
  }
  return 1;
}
int main(void) {
  CHECK(increasing(data, 4));
  CHECK(increasing(data, 2));
  CHECK(increasing(data, 0));
  return 0;
}
