// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static int lt(u8 a, u8 b) { return a < b; }
static int ge(u8 a, u8 b) { return a >= b; }
int main(void) {
  CHECK(lt(0, 255));
  CHECK(!lt(255, 0));
  CHECK(lt(127, 128));
  CHECK(ge(128, 127));
  CHECK(ge(200, 200));
  CHECK(!ge(1, 2));
  return 0;
}
