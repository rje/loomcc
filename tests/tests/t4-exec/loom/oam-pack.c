// loomcc-do: run
// loomcc-int: agnostic
// Packing sprite attributes into OAM bytes: x low byte, y, tile, attribute
// (palette, priority, flips) and the high table's x bit 8 and size bit.
#include "loomcc-test.h"
typedef struct { i16 x; i16 y; u16 tile; u8 pal; u8 prio; u8 hflip; u8 vflip; u8 big; } Sprite;
static u8 oam_lo[4 * 4];
static u8 oam_hi[1];
static void pack(const Sprite *s, u8 slot) {
  u8 *o = &oam_lo[slot * 4];
  u8 shift = (u8)(slot * 2);
  o[0] = (u8)s->x;
  o[1] = (u8)s->y;
  o[2] = (u8)s->tile;
  o[3] = (u8)(((s->tile >> 8) & 1) | ((s->pal & 7) << 1) | ((s->prio & 3) << 4) | (s->hflip ? 0x40 : 0) | (s->vflip ? 0x80 : 0));
  oam_hi[0] = (u8)((oam_hi[0] & ~(3 << shift)) | ((((u16)s->x >> 8) & 1) << shift) | ((s->big ? 1 : 0) << (shift + 1)));
}
int main(void) {
  static const Sprite sprites[4] = {
    { 10, 20, 0x001, 0, 3, 0, 0, 0 }, { -3, 200, 0x1ff, 7, 2, 1, 0, 1 },
    { 256, 0, 0x100, 5, 0, 0, 1, 0 }, { 300, 223, 0x080, 2, 1, 1, 1, 1 },
  };
  u8 i;
  for (i = 0; i < 4; i++) pack(&sprites[i], i);
  CHECK(oam_lo[0] == 10 && oam_lo[1] == 20 && oam_lo[2] == 1 && oam_lo[3] == 0x30);
  CHECK(oam_lo[4] == 0xfd && oam_lo[5] == 200 && oam_lo[6] == 0xff && oam_lo[7] == (1 | 14 | 0x20 | 0x40));
  CHECK(oam_lo[8] == 0 && oam_lo[10] == 0 && oam_lo[11] == (1 | 10 | 0x80));
  CHECK(oam_lo[12] == 44 && oam_lo[15] == (4 | 0x10 | 0x40 | 0x80));
  CHECK(oam_hi[0] == (u8)(0 | (3 << 2) | (1 << 4) | (3 << 6)));
  return 0;
}
