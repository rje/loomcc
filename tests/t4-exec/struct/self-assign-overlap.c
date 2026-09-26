// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
struct Big { i16 v[6]; };
int main(void) {
  struct Big a[2];
  int i;
  for (i = 0; i < 6; i++) { a[0].v[i] = (i16)i; a[1].v[i] = (i16)(10 + i); }
  a[0] = a[0];
  a[1] = a[0];
  CHECK(a[1].v[5] == 5 && a[0].v[5] == 5);
  return 0;
}
