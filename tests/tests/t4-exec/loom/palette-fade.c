// loomcc-do: run
// loomcc-int: agnostic
// BGR555 colours faded by a 0-16 level: split, scale, round, repack.
#include "loomcc-test.h"
static u16 fade(u16 c, u8 level) {
  u8 r = (u8)(c & 31), g = (u8)((c >> 5) & 31), b = (u8)((c >> 10) & 31);
  r = (u8)((r * level + 8) >> 4);
  g = (u8)((g * level + 8) >> 4);
  b = (u8)((b * level + 8) >> 4);
  return (u16)(r | (g << 5) | ((u16)b << 10));
}
int main(void) {
  CHECK(fade(0x7fff, 16) == 0x7fff);
  CHECK(fade(0x7fff, 0) == 0);
  CHECK(fade(0x7fff, 8) == (u16)(16 | (16 << 5) | (16 << 10)));
  CHECK(fade(0x001f, 4) == 8);
  CHECK(fade(0x03e0, 12) == (u16)(23 << 5));
  CHECK(fade(0x7c00, 1) == (u16)(2 << 10));
  return 0;
}
