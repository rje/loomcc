// loomcc-do: run
// loomcc-int: agnostic
// A loop whose counter reaches 65535 without the condition overflowing.
#include "loomcc-test.h"
int main(void) {
  u16 i;
  u16 n = 0;
  for (i = 65530u; i != 0; i++) n++;
  CHECK(n == 6);
  return 0;
}
