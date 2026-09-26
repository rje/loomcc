// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  i16 a[10];
  i16 *p = a, *q = &a[9];
  int i;
  for (i = 0; i < 10; i++) a[i] = (i16)i;
  CHECK(q - p == 9);
  CHECK(*(p + 4) == 4);
  CHECK(*(q - 2) == 7);
  p += 3; CHECK(*p == 3);
  p++; CHECK(*p == 4);
  --q; CHECK(*q == 8);
  CHECK(p < q && q > p && p != q);
  CHECK(&p[1] == p + 1);
  return 0;
}
