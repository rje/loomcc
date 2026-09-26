// loomcc-do: run
// loomcc-int: 16
// loomcc-tcc-sources: aux/tcc-u8.c
// 8-bit parameters occupy one byte each in the 816-tcc ABI, in both
// directions, mixed with 16-bit and pointer parameters.
#include "loomcc-test.h"
u16 tcc_u8_sum(u8 a, u8 b, u16 c, u8 d, u8 *p, i8 e);
u16 unit_u8_sum(u8 a, u8 b, u16 c, u8 d, u8 *p, i8 e) { return (u16)(a + b + c + d + *p + e); }
u16 tcc_calls_unit(void);
int main(void) {
  u8 x = 200;
  CHECK(tcc_u8_sum(1, 255, 1000, 2, &x, -3) == 1 + 255 + 1000 + 2 + 200 - 3);
  CHECK(tcc_calls_unit() == 10 + 20 + 30 + 40 + 50 - 60);
  return 0;
}
