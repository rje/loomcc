// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static u16 rol(u16 x, int n) { return (u16)((x << n) | (x >> (16 - n))); }
static u8 rol8(u8 x, int n) { return (u8)((x << n) | (x >> (8 - n))); }
int main(void) {
  CHECK(rol(0x8001, 1) == 0x0003);
  CHECK(rol(0x1234, 4) == 0x2341);
  CHECK(rol(0x1234, 8) == 0x3412);
  CHECK(rol8(0x81, 1) == 0x03);
  CHECK(rol8(0x12, 4) == 0x21);
  return 0;
}
