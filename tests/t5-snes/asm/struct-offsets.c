// loomcc-do: run
// loomcc-int: 16
// loomcc-asm-sources: aux/read-struct.asm
// loomcc-skip-mode: ir
// Hand-written assembly reads a struct by fixed offsets, as Loom's body.asm
// and oam.asm do: the layout must be 816-tcc's.
#include "loomcc-test.h"
struct Body { u8 flags; u8 *sprite; i16 x; i16 y; u8 frame; };
u16 asm_body_sum(struct Body *b);   /* flags + x + y + frame + sprite[0] */
int main(void) {
  static u8 spr[2] = { 40, 41 };
  struct Body b;
  b.flags = 1; b.sprite = spr; b.x = 100; b.y = -30; b.frame = 2;
  CHECK(asm_body_sum(&b) == (u16)(1 + 100 - 30 + 2 + 40));
  return 0;
}
