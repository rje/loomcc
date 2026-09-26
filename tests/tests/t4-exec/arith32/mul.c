// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static i32 mul(i32 a, i32 b) { return a * b; }
static u32 umul(u32 a, u32 b) { return a * b; }
int main(void) {
  CHECK(mul(300, 300) == 90000);
  CHECK(mul(-300, 300) == -90000);
  CHECK(mul(46340, 46340) == 2147395600);
  CHECK(mul(65536, 16) == 1048576);
  CHECK(umul(65535u, 65537u) == 0xffffffffu);
  CHECK(umul(0x10000u, 0x10000u) == 0);
  CHECK(umul(0xdeadbeefu, 3) == 0x9c093ccdu);
  return 0;
}
