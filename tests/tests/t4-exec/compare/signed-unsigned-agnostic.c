// loomcc-do: run
// loomcc-int: agnostic
// u8 and i8 promote to int everywhere: these comparisons are signed.
#include "loomcc-test.h"
int main(void) {
  volatile i16 m = -1;
  volatile u8 one = 1;
  CHECK(m < one);
  CHECK((i8)-1 < (u8)1);
  CHECK((u8)255 > (i8)-1);
  return 0;
}
