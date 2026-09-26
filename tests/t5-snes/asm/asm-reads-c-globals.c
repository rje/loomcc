// loomcc-do: run
// loomcc-int: 16
// loomcc-asm-sources: aux/read-globals.asm
// loomcc-ref: tcc-rom
// loomcc-skip-mode: ir
// Assembly reads and writes C globals by label and fixed offsets, as Loom's
// body.asm and oam.asm do: the globals must keep their C names, live where
// 816-tcc puts them (.bss in bank $7E: `lda.w label`) and use its layout.
#include "loomcc-test.h"
typedef struct { i16 x; u8 flags; u8 *sprite; i16 y; } Body;   /* x 0, flags 2, sprite 4, y 8; 12 bytes */
Body bodies[3];
u16 body_count;
u16 asm_sum_x_plus_y(void);      /* sum of x + y over body_count bodies */
void asm_set_flags(u8 v);        /* flags of every body = v */
int main(void) {
  static u8 spr = 1;
  u8 i;
  STATIC_CHECK(sizeof(Body) == 12);
  for (i = 0; i < 3; i++) { bodies[i].x = (i16)(i * 100 - 50); bodies[i].y = (i16)(i + 1); bodies[i].sprite = &spr; bodies[i].flags = 0; }
  body_count = 3;
  CHECK(asm_sum_x_plus_y() == (u16)(-50 + 50 + 150 + 1 + 2 + 3));
  asm_set_flags(0xa5);
  CHECK(bodies[0].flags == 0xa5 && bodies[2].flags == 0xa5 && bodies[2].y == 3 && bodies[1].sprite == &spr);
  return 0;
}
