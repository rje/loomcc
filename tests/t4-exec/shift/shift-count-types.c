// loomcc-do: run
// loomcc-int: agnostic
// The shift count's type does not affect the result type.
#include "loomcc-test.h"
int main(void) {
  volatile u8 n8 = 3;
  volatile i32 n32 = 4;
  volatile u16 v = 0x0101;
  CHECK((u16)(v << n8) == 0x0808);
  CHECK((u16)(v << n32) == 0x1010);
  CHECK((u16)(v >> n8) == 0x0020);
  CHECK(sizeof(v << n32) == sizeof(int));
  return 0;
}
