// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  int a = 0;
  CHECK(sizeof(a++) == sizeof(int));
  CHECK(a == 0);
  return 0;
}
