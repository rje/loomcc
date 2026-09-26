// loomcc-do: run
// loomcc-int: agnostic
// (a / b) * b + a % b == a for every sign combination.
#include "loomcc-test.h"
int main(void) {
  static const i16 as[] = { 0, 1, -1, 7, -7, 100, -100, 32767, -32767, -32768 };
  static const i16 bs[] = { 1, -1, 2, -2, 3, -3, 7, -7, 10, 255, -256, 32767 };
  unsigned i, j;
  for (i = 0; i < sizeof as / sizeof as[0]; i++)
    for (j = 0; j < sizeof bs / sizeof bs[0]; j++) {
      i16 a = as[i], b = bs[j];
      if (a == -32768 && b == -1) continue;     /* overflows */
      CHECK((i16)((a / b) * b + a % b) == a);
      CHECK((a % b == 0) || ((a % b < 0) == (a < 0)));
    }
  return 0;
}
