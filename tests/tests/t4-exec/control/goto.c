// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  int i = 0, n = 0;
again:
  n += i;
  if (++i < 5) goto again;
  CHECK(n == 10);
  goto skip;
  n = -1;
skip:
  CHECK(n == 10);
  return 0;
}
