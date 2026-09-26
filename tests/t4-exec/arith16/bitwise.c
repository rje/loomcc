// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  volatile u16 a = 0xa5c3, b = 0x0ff0;
  CHECK((a & b) == 0x05c0);
  CHECK((a | b) == 0xaff3);
  CHECK((a ^ b) == 0xaa33);
  CHECK((u16)~a == 0x5a3c);
  CHECK((a & 0xff) == 0xc3);
  CHECK((a >> 8) == 0xa5);
  return 0;
}
