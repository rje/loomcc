// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static i16 neg(i16 x) { return (i16)-x; }
int main(void) {
  CHECK(neg(1) == -1);
  CHECK(neg(-1) == 1);
  CHECK(neg(0) == 0);
  CHECK(neg(32767) == -32767);
  CHECK(neg(-32767) == 32767);
  return 0;
}
