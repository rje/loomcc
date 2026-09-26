// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  int i, j, n = 0;
  for (i = 0; i < 5; i++)
    for (j = 0; j < 5; j++) {
      if (j == i) break;
      n++;
    }
  CHECK(n == 10);
  return 0;
}
