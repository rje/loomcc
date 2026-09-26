// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  volatile u8 a = 0xa5, b = 0x3c;
  CHECK((u8)(a & b) == 0x24);
  CHECK((u8)(a | b) == 0xbd);
  CHECK((u8)(a ^ b) == 0x99);
  CHECK((u8)~a == 0x5a);
  CHECK(~a == -0xa6);          /* ~ on the promoted int */
  CHECK((a >> 4) == 0x0a);
  CHECK((u8)(a << 1) == 0x4a);
  return 0;
}
