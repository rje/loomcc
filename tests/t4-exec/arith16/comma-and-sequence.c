// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  i16 a = 1, b;
  b = (a += 2, a * 10);
  CHECK(a == 3 && b == 30);
  return 0;
}
