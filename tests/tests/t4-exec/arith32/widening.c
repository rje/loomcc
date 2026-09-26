// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  volatile i16 s = -2;
  volatile u16 u = 65534;
  volatile i8 c = -3;
  i32 a = s;
  u32 b = u;
  i32 d = c;
  u32 e = (u16)s;
  CHECK(a == -2);
  CHECK(b == 65534);
  CHECK(d == -3);
  CHECK(e == 65534);
  CHECK((i32)u * 2 == 131068);
  CHECK((u32)s == 0xfffffffeu);
  return 0;
}
