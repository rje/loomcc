// loomcc-do: run
// loomcc-int: 16
// loomcc-skip-mode: host16
// loomcc-extra-sources: aux/far0.c aux/far1.c aux/far2.c
// A function returns a pointer to its unit's static const ROM table; the
// tables are 20000 bytes each, so at least two live outside the code's bank
// and the pointers must carry their banks.
#include "loomcc-test.h"
const u8 *far_ptr0(void);
const u8 *far_ptr1(void);
const u8 *far_ptr2(void);
int main(void) {
  const u8 *p[3];
  u8 k;
  p[0] = far_ptr0(); p[1] = far_ptr1(); p[2] = far_ptr2();
  for (k = 0; k < 3; k++) CHECK(p[k][0] == 10 * (k + 1) && p[k][19999u] == 10 * (k + 1) + 1);
  return 0;
}
