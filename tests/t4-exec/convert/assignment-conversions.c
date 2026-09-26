// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static u8 ret_u8(i16 x) { return (u8)x; }
static i8 ret_i8(u16 x) { return (i8)x; }
static i32 ret_i32(i16 x) { return x; }
static u32 ret_u32(u16 x) { return x; }
int main(void) {
  CHECK(ret_u8(-1) == 255);
  CHECK(ret_u8(0x1234) == 0x34);
  CHECK(ret_i8(0xff80) == -128);
  CHECK(ret_i32(-5) == -5);
  CHECK(ret_u32(0xffff) == 0xffffu);
  return 0;
}
