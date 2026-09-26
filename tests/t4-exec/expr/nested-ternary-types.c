// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static i32 pick(int k) { return k == 0 ? (i8)-1 : k == 1 ? (u8)255 : (i32)70000; }
int main(void) {
  CHECK(pick(0) == -1);
  CHECK(pick(1) == 255);
  CHECK(pick(2) == 70000);
  return 0;
}
