// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  volatile u32 big = 0x10000u;
  volatile u16 w = 0x100;
  volatile i8 c = -128;
  volatile u8 *p = 0;
  CHECK(big ? 1 : 0);
  CHECK((u16)big == 0);
  CHECK(!!w == 1);
  CHECK(!!(u8)w == 0);
  CHECK(c && 1);
  CHECK(!p);
  if (big) {} else CHECK(0);
  return 0;
}
