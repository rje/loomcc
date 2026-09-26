// loomcc-do: run
// loomcc-ref-diverges: tcc-rom [tcc-no-llshift] 816-tcc calls tcc__ashldi3 for variable 32-bit shifts and PVSnesLib's libtcc lacks it
// loomcc-int: agnostic
#include "loomcc-test.h"
static u32 shl(u32 a, int n) { return a << n; }
static u32 shr(u32 a, int n) { return a >> n; }
static i32 sar(i32 a, int n) { return a >> n; }
int main(void) {
  CHECK(shl(1, 0) == 1);
  CHECK(shl(1, 15) == 0x8000u);
  CHECK(shl(1, 16) == 0x10000u);
  CHECK(shl(1, 31) == 0x80000000u);
  CHECK(shl(0x12345678u, 8) == 0x34567800u);
  CHECK(shr(0x80000000u, 31) == 1);
  CHECK(shr(0x12345678u, 16) == 0x1234u);
  CHECK(shr(0x12345678u, 4) == 0x01234567u);
  CHECK(sar(-65536, 16) == -1);
  CHECK(sar(-1, 31) == -1);
  CHECK(sar(0x40000000, 30) == 1);
  return 0;
}
