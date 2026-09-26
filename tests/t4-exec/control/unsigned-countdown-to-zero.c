// loomcc-do: run
// loomcc-int: agnostic
// The unsigned count-down idiom: i-- > 0 visits n-1 .. 0.
#include "loomcc-test.h"
int main(void) {
  u8 i = 5;
  u16 sum = 0, n = 0;
  while (i-- > 0) { sum = (u16)(sum + i); n++; }
  CHECK(n == 5 && sum == 10 && i == 255);
  return 0;
}
