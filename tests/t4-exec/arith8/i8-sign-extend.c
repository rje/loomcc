// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  volatile i8 a = -1;
  volatile i8 b = -128;
  i16 w = a;
  CHECK(w == -1);
  w = b;
  CHECK(w == -128);
  CHECK(a < 0);
  CHECK(b < a);
  CHECK((u8)a == 255);
  CHECK((u16)(i16)a == 65535u);
  return 0;
}
