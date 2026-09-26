// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  volatile u32 a = 0x12345678u;
  CHECK((u16)a == 0x5678);
  CHECK((u8)a == 0x78);
  CHECK((u16)(a >> 16) == 0x1234);
  volatile i32 b = -70000;
  CHECK((i16)b == (i16)(u16)(0xfffeee90u & 0xffff));
  CHECK((u16)b == 0xee90);
  return 0;
}
