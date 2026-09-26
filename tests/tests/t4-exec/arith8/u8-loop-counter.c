// loomcc-do: run
// loomcc-int: agnostic
// A u8 counter that wraps: the loop runs 256 times.
#include "loomcc-test.h"
int main(void) {
  u8 i = 0;
  u16 n = 0;
  do { n++; i++; } while (i != 0);
  CHECK(n == 256);
  return 0;
}
