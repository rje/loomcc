// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  u16 a[8];
  u16 *p;
  int i;
  for (i = 0; i < 8; i++) a[i] = (u16)(1u << i);
  i = 0;
  for (p = &a[7]; p >= &a[0] && i < 8; p--, i++) CHECK(*p == (u16)(1u << (7 - i)));
  CHECK(i == 8);
  return 0;
}
