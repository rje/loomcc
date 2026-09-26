// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  volatile u8 a = 0x81;
  volatile i8 s = -128;
  u8 r;
  r = (u8)(a << 1); CHECK(r == 0x02);
  r = (u8)(a >> 1); CHECK(r == 0x40);
  CHECK((a << 1) == 0x102);            /* promoted to int first */
  CHECK((s >> 1) == -64);
  CHECK((s >> 7) == -1);
  CHECK(((u8)s >> 7) == 1);
  return 0;
}
