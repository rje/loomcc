// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  volatile i16 a = 32767, b = -32768, c = 181;
  CHECK(a - 1 == 32766);
  CHECK(b + 1 == -32767);
  CHECK(c * c == 32761);
  CHECK(-a == -32767);
  CHECK(a + b == -1);
  return 0;
}
