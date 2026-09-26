// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  volatile u16 a = 0x8421;
  CHECK((u16)(a << 0) == 0x8421);
  CHECK((u16)(a << 1) == 0x0842);
  CHECK((u16)(a << 4) == 0x4210);
  CHECK((u16)(a << 8) == 0x2100);
  CHECK((u16)(a << 15) == 0x8000);
  CHECK((a >> 1) == 0x4210);
  CHECK((a >> 7) == 0x0108);
  CHECK((a >> 8) == 0x84);
  CHECK((a >> 15) == 1);
  return 0;
}
