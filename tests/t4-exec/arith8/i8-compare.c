// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static int lt(i8 a, i8 b) { return a < b; }
int main(void) {
  CHECK(lt(-128, 127));
  CHECK(!lt(127, -128));
  CHECK(lt(-1, 0));
  CHECK(lt(-2, -1));
  CHECK(!lt(0, 0));
  return 0;
}
