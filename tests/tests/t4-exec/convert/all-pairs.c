// loomcc-do: run
// loomcc-int: agnostic
// Every conversion between the six integer widths, from boundary values.
// Out-of-range conversions to signed types are implementation-defined:
// modulo 2^N (two's complement) on every target here.
#include "loomcc-test.h"
int main(void) {
  volatile i8 s8 = -128; volatile u8 u8v = 0xff;
  volatile i16 s16 = -32768; volatile u16 u16v = 0xffff;
  volatile i32 s32 = -2147483647 - 1; volatile u32 u32v = 0xffffffffu;
  CHECK((u8)s8 == 0x80 && (i16)s8 == -128 && (u16)s8 == 0xff80 && (i32)s8 == -128 && (u32)s8 == 0xffffff80u);
  CHECK((i8)u8v == -1 && (i16)u8v == 255 && (u16)u8v == 255 && (i32)u8v == 255 && (u32)u8v == 255);
  CHECK((i8)s16 == 0 && (u8)s16 == 0 && (u16)s16 == 0x8000 && (i32)s16 == -32768 && (u32)s16 == 0xffff8000u);
  CHECK((i8)u16v == -1 && (u8)u16v == 0xff && (i16)u16v == -1 && (i32)u16v == 65535 && (u32)u16v == 65535);
  CHECK((i8)s32 == 0 && (u8)s32 == 0 && (i16)s32 == 0 && (u16)s32 == 0 && (u32)s32 == 0x80000000u);
  CHECK((i8)u32v == -1 && (u8)u32v == 0xff && (i16)u32v == -1 && (u16)u32v == 0xffff && (i32)u32v == -1);
  return 0;
}
