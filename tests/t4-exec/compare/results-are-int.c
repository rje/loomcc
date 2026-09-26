// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  volatile i16 a = 3, b = 5;
  i16 r = (a < b) + (a == 3) + (b != 5) + (a >= b);
  CHECK(r == 2);
  CHECK((a < b) * 100 == 100);
  return 0;
}
