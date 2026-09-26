// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static int lt(i32 a, i32 b) { return a < b; }
static int ult(u32 a, u32 b) { return a < b; }
int main(void) {
  CHECK(lt(-1, 0));
  CHECK(lt(-70000, -69999));
  CHECK(!lt(65536, 65535));
  CHECK(lt(65535, 65536));
  CHECK(lt(-2147483647 - 1, 2147483647));
  CHECK(ult(0, 0xffffffffu));
  CHECK(!ult(0x80000000u, 0x7fffffffu));
  CHECK(ult(0xffffu, 0x10000u));
  return 0;
}
