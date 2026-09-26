// loomcc-do: run
// loomcc-int: 16
// loomcc-skip-mode: host16
// loomcc-extra-sources: aux/far0.c aux/far1.c aux/far2.c
// Three units, each with a 20000-byte table and a function reading it: code
// and data end up in different banks, reached by long calls, long data reads
// and function pointers that carry the bank.
#include "loomcc-test.h"
u16 far_sum0(u16 from, u16 n);
u16 far_sum1(u16 from, u16 n);
u16 far_sum2(u16 from, u16 n);
static u16 (*const sums[3])(u16, u16) = { far_sum0, far_sum1, far_sum2 };
int main(void) {
  u8 k;
  CHECK(far_sum0(0, 1) == 10 && far_sum1(19999, 1) == 21 && far_sum2(0, 20000) == 30 + 31);
  for (k = 0; k < 3; k++) CHECK(sums[k](19990u, 10) == (u16)(10 * (k + 1) + 1));
  return 0;
}
