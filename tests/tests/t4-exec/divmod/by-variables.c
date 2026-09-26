// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static i16 dv(i16 a, i16 b) { return (i16)(a / b); }
static i16 md(i16 a, i16 b) { return (i16)(a % b); }
static u16 udv(u16 a, u16 b) { return (u16)(a / b); }
static u16 umd(u16 a, u16 b) { return (u16)(a % b); }
int main(void) {
  CHECK(dv(100, 7) == 14 && md(100, 7) == 2);
  CHECK(dv(-100, 7) == -14 && md(-100, 7) == -2);
  CHECK(dv(100, -7) == -14 && md(100, -7) == 2);
  CHECK(dv(-100, -7) == 14 && md(-100, -7) == -2);
  CHECK(dv(32767, 1) == 32767);
  CHECK(dv(-32768, 1) == -32768);
  CHECK(dv(5, 10) == 0 && md(5, 10) == 5);
  CHECK(udv(65535u, 1) == 65535u);
  CHECK(udv(65535u, 255) == 257);
  CHECK(umd(65535u, 256) == 255);
  CHECK(udv(40000u, 3) == 13333 && umd(40000u, 3) == 1);
  CHECK(udv(1, 65535u) == 0 && umd(1, 65535u) == 1);
  return 0;
}
