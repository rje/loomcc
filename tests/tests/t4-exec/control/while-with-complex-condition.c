// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static const i16 v[] = { 3, 5, -1, 8, 0, 9 };
int main(void) {
  u8 i = 0;
  i16 s = 0;
  while (i < 6 && v[i] != 0 && (s += v[i], s < 100)) i++;
  CHECK(i == 4 && s == 15);
  return 0;
}
