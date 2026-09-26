// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
struct Frame { u8 tile; i8 dx; i8 dy; u8 flags; };
static const struct Frame anim[] = {
  { 1, -8, -16, 0x10 }, { 2, 0, -16, 0x10 }, { 3, -8, 0, 0x20 }, { 4, 0, 0, 0x20 }
};
int main(void) {
  i16 sx = 0, sy = 0;
  unsigned i;
  u8 f = 0;
  for (i = 0; i < sizeof anim / sizeof anim[0]; i++) { sx += anim[i].dx; sy += anim[i].dy; f |= anim[i].flags; }
  CHECK(sx == -16 && sy == -32 && f == 0x30);
  CHECK(anim[2].tile == 3);
  return 0;
}
