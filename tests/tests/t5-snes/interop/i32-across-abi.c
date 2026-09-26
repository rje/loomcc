// loomcc-do: run
// loomcc-int: 16
// loomcc-tcc-sources: aux/tcc-i32.c
// 32-bit integers cross the ABI as 4-byte slots and come back in
// tcc__r0/tcc__r0h: loomcc's long and 816-tcc's long long (the harness i32).
#include "loomcc-test.h"
i32 tcc_i32_add(i32 a, i32 b);
u32 tcc_u32_mix(u8 a, u32 b, i16 c);
i32 unit_i32_scale(i32 v, i16 k) { return v * k; }
i32 tcc_calls_scale(i32 v);
int main(void) {
  CHECK(tcc_i32_add(100000, -30000) == 70000);
  CHECK(tcc_u32_mix(7, 0x12345678ul, -1) == 0x12345678ul + 7 - 1);
  CHECK(tcc_calls_scale(-70000) == -210000);
  return 0;
}
