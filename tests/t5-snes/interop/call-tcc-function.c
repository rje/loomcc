// loomcc-do: run
// loomcc-int: 16
// loomcc-tcc-sources: aux/tcc-math.c
// The unit under test calls functions compiled by 816-tcc (816-tcc ABI:
// arguments pushed right to left, 1/2/4-byte slots, result in tcc__r0).
#include "loomcc-test.h"
i16 tcc_add(i16 a, i16 b);
u8 tcc_low(u16 v);
i16 tcc_mix(u8 a, i16 b, u8 c, char *p);
u16 tcc_ptr_diff(u8 *a, u8 *b);
int main(void) {
  static u8 buf[10];
  char c = 5;
  CHECK(tcc_add(1000, -3) == 997);
  CHECK(tcc_low(0x1234) == 0x34);
  CHECK(tcc_mix(200, -1000, 7, &c) == 200 - 1000 + 7 + 5);
  CHECK(tcc_ptr_diff(&buf[9], &buf[2]) == 7);
  return 0;
}
