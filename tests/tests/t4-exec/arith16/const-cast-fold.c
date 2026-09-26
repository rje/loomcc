// loomcc-do: run
// loomcc-int: agnostic
// Conversions the compiler folds at compile time must agree with the same
// conversions done at run time.
// loomcc-ref-diverges: tcc-rom [tcc-fold-host-int] 816-tcc folds (i16)(u16)65535u to 65535, not -1
#include "loomcc-test.h"
int main(void) {
  volatile u16 m = 65535u;
  CHECK((i16)m == -1);
  CHECK((i16)(u16)65535u == -1);
  CHECK((i16)(u16)65535u == (i16)m);
  CHECK((i8)(u8)200 == -56);
  CHECK((u16)(i16)-1 == 65535u);
  return 0;
}
