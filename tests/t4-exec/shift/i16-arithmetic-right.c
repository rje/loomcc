// loomcc-do: run
// loomcc-int: agnostic
// loomcc-note: >> of a negative value is implementation-defined; every
// loomcc-note: compiler here shifts arithmetically, and loomcc must too.
#include "loomcc-test.h"
static i16 sar(i16 a, int n) { return (i16)(a >> n); }
int main(void) {
  CHECK(sar(-1, 1) == -1);
  CHECK(sar(-2, 1) == -1);
  CHECK(sar(-32768, 15) == -1);
  CHECK(sar(-32768, 8) == -128);
  CHECK(sar(-256, 4) == -16);
  CHECK(sar(32767, 14) == 1);
  CHECK(sar(-3, 1) == -2);
  return 0;
}
