// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  int a = 0, b = 0, r;
  r = 1 ? a++ : b++;
  CHECK(r == 0 && a == 1 && b == 0);
  r = 0 ? a++ : b++;
  CHECK(r == 0 && a == 1 && b == 1);
  r = (a > b) ? a : b;
  CHECK(r == 1);
  return 0;
}
