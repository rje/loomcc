// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static u16 shl(u16 a, int n) { return (u16)(a << n); }
static u16 shr(u16 a, int n) { return (u16)(a >> n); }
int main(void) {
  int i;
  for (i = 0; i < 16; i++) {
    CHECK(shl(1, i) == (u16)(1u << i));
    CHECK(shr(0x8000u, i) == (u16)(0x8000u >> i));
  }
  CHECK(shl(0xffff, 8) == 0xff00);
  CHECK(shr(0xffff, 12) == 0x000f);
  return 0;
}
