// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static int lt(u16 a, u16 b) { return a < b; }
static int ge(u16 a, u16 b) { return a >= b; }
int main(void) {
  static const u16 v[] = { 0, 1, 255, 256, 32767, 32768, 32769, 65534, 65535u };
  unsigned i, j;
  for (i = 0; i < 9; i++)
    for (j = 0; j < 9; j++) {
      CHECK(lt(v[i], v[j]) == (i < j));
      CHECK(ge(v[i], v[j]) == (i >= j));
    }
  return 0;
}
