// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static i32 dv(i32 a, i32 b) { return a / b; }
static i32 md(i32 a, i32 b) { return a % b; }
static u32 udv(u32 a, u32 b) { return a / b; }
static u32 umd(u32 a, u32 b) { return a % b; }
int main(void) {
  CHECK(dv(1000000, 7) == 142857);
  CHECK(md(1000000, 7) == 1);
  CHECK(dv(-1000000, 7) == -142857);
  CHECK(md(-1000000, 7) == -1);
  CHECK(dv(1000000, -7) == -142857);
  CHECK(md(1000000, -7) == 1);
  CHECK(udv(0xffffffffu, 0x10000u) == 0xffffu);
  CHECK(umd(0xffffffffu, 0x10000u) == 0xffffu);
  CHECK(udv(0x80000000u, 3) == 0x2aaaaaaau);
  CHECK(umd(0x80000000u, 3) == 2);
  CHECK(udv(12345, 12346) == 0);
  return 0;
}
