// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  i16 a[12] = { 5, -3, 9, 0, -32768, 32767, 7, 7, -1, 2, 100, -100 };
  int i, j;
  for (i = 0; i < 12; i++)
    for (j = 0; j + 1 < 12 - i; j++)
      if (a[j] > a[j + 1]) { i16 t = a[j]; a[j] = a[j + 1]; a[j + 1] = t; }
  for (i = 0; i + 1 < 12; i++) CHECK(a[i] <= a[i + 1]);
  CHECK(a[0] == -32768 && a[11] == 32767);
  return 0;
}
