// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static i16 counter(void) { static i16 n = 100; return n++; }
int main(void) {
  CHECK(counter() == 100);
  CHECK(counter() == 101);
  CHECK(counter() == 102);
  return 0;
}
