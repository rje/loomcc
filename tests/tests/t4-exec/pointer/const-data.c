// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static const i16 tab[] = { 5, -5, 50, -50 };
static i16 sum(const i16 *p, int n) { i16 s = 0; while (n--) s += *p++; return s; }
int main(void) {
  CHECK(sum(tab, 4) == 0);
  CHECK(sum(tab + 2, 1) == 50);
  return 0;
}
