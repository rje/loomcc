// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static i16 one(void) { return 1; }
static i16 two(void) { return 2; }
static i16 (*pick(int which))(void) { return which ? two : one; }
int main(void) {
  CHECK(pick(0)() == 1);
  CHECK(pick(1)() == 2);
  return 0;
}
