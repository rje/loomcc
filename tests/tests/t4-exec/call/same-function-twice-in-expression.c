// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static i16 sq(i16 x) { i16 t = (i16)(x * x); return t; }
int main(void) {
  CHECK(sq(3) + sq(4) == 25);
  CHECK(sq(sq(2)) == 16);
  CHECK(sq(2) * sq(3) - sq(1) == 35);
  return 0;
}
