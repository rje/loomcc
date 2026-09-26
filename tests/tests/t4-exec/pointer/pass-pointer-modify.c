// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static void swap(i16 *x, i16 *y) { i16 t = *x; *x = *y; *y = t; }
static void minmax(const i16 *a, int n, i16 *lo, i16 *hi) {
  int i;
  *lo = *hi = a[0];
  for (i = 1; i < n; i++) { if (a[i] < *lo) *lo = a[i]; if (a[i] > *hi) *hi = a[i]; }
}
int main(void) {
  i16 x = 1, y = 2, lo, hi;
  static const i16 v[] = { 5, -3, 12, 0, -7, 8 };
  swap(&x, &y);
  CHECK(x == 2 && y == 1);
  minmax(v, 6, &lo, &hi);
  CHECK(lo == -7 && hi == 12);
  return 0;
}
