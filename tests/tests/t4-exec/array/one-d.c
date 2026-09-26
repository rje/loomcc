// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  i16 a[16];
  int i;
  for (i = 0; i < 16; i++) a[i] = (i16)(i * 3 - 20);
  CHECK(a[0] == -20 && a[15] == 25);
  CHECK(sizeof(a) == 16 * sizeof(i16));
  for (i = 15; i > 0; i--) a[i] = a[i - 1];
  CHECK(a[1] == -20 && a[15] == 22);
  return 0;
}
