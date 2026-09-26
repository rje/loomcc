// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  volatile u16 a = 65535;
  u16 b = a;
  b++;
  CHECK(b == 0);
  b--;
  CHECK(b == 65535u);
  b = (u16)(a + 2);
  CHECK(b == 1);
  b = (u16)(1u * a * a);          /* 1u *: unsigned on every host */
  CHECK(b == 1);
  b = (u16)(0u - a);
  CHECK(b == 1);
  return 0;
}
