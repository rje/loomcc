// loomcc-do: run
// loomcc-int: agnostic
// Signed comparisons across the sign boundary (the 65816's CMP sets carry for
// unsigned order only; signed order needs overflow handling).
#include "loomcc-test.h"
static int lt(i16 a, i16 b) { return a < b; }
static int le(i16 a, i16 b) { return a <= b; }
static int gt(i16 a, i16 b) { return a > b; }
static int ge(i16 a, i16 b) { return a >= b; }
int main(void) {
  static const i16 v[] = { -32768, -32767, -256, -1, 0, 1, 255, 256, 32766, 32767 };
  unsigned i, j;
  for (i = 0; i < 10; i++)
    for (j = 0; j < 10; j++) {
      CHECK(lt(v[i], v[j]) == (i < j));
      CHECK(le(v[i], v[j]) == (i <= j));
      CHECK(gt(v[i], v[j]) == (i > j));
      CHECK(ge(v[i], v[j]) == (i >= j));
    }
  return 0;
}
