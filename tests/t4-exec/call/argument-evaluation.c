// loomcc-do: run
// loomcc-int: agnostic
// Arguments are all evaluated before the call (order unspecified: no side effects shared).
#include "loomcc-test.h"
static i16 add3(i16 a, i16 b, i16 c) { return (i16)(a * 100 + b * 10 + c); }
static i16 id(i16 x) { return x; }
int main(void) {
  CHECK(add3(id(1), id(2), id(3)) == 123);
  CHECK(add3(add3(0, 0, 1), id(2), add3(0, 0, 3)) == 123);
  return 0;
}
