// loomcc-do: run
// loomcc-int: agnostic
// 8-bit arguments occupy one byte each in the 816-tcc ABI; many in a row.
#include "loomcc-test.h"
static u16 pack(u8 a, u8 b, u8 c, u8 d, u8 e, u8 f) { return (u16)(a + (b << 1) + (c << 2) + (d << 3) + (e << 4) + (f << 5)); }
static i16 mixed(u8 a, i16 b, u8 c, i16 d) { return (i16)(a - b + c - d); }
int main(void) {
  CHECK(pack(1, 1, 1, 1, 1, 1) == 63);
  CHECK(pack(255, 0, 0, 0, 0, 1) == 255 + 32);
  CHECK(mixed(200, -1000, 55, 300) == 200 + 1000 + 55 - 300);
  return 0;
}
