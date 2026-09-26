// loomcc-do: run
// loomcc-int: agnostic
// A 32-bit counter crossing 16-bit boundaries.
#include "loomcc-test.h"
int main(void) {
  u32 c = 0xfffffff0u;
  u16 i;
  for (i = 0; i < 32; i++) c++;
  CHECK(c == 0x10);
  c = 0x0000fff8u;
  for (i = 0; i < 16; i++) c++;
  CHECK(c == 0x00010008u);
  c = 0x00010000u;
  c--;
  CHECK(c == 0xffffu);
  return 0;
}
