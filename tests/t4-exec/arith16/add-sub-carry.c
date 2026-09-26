// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  volatile u16 a = 0x00ff, b = 0x0001, c = 0xff00;
  CHECK((u16)(a + b) == 0x0100);
  CHECK((u16)(c + a) == 0xffff);
  CHECK((u16)(c + 0x100) == 0);
  CHECK((u16)(b - a) == 0xff02);
  CHECK((u16)(0 - c) == 0x0100);
  return 0;
}
