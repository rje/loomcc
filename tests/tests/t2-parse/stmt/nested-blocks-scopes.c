// loomcc-do: syntax
#include "loomcc-test.h"
int x;
int f(void) {
  char x;
  { long x; STATIC_CHECK(sizeof(x) == sizeof(long)); { short x; STATIC_CHECK(sizeof(x) == 2); } }
  STATIC_CHECK(sizeof(x) == 1);
  return 0;
}
STATIC_CHECK(sizeof(x) == sizeof(int));
