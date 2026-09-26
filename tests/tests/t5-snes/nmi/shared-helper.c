// loomcc-do: run
// loomcc-int: 16
// loomcc-ref: tcc-rom
// loomcc-skip-mode: ir
// A VBlank handler registered with nmiSet() and the main loop both call the
// same helper while the main loop keeps values live across it. Code reached
// from NMI must not share static frames with code it interrupts (loomcc
// PLAN section 9: nmiSet's argument is an interrupt root).
#include "loomcc-test.h"
void nmiSet(void (*vblankRoutine)(void));
static volatile u16 ticks;
static volatile u16 nmi_acc;
static u16 mix(u16 a, u16 b) {
  u16 t = (u16)(a * 3u + b);
  u16 u = (u16)(t ^ (t >> 3));
  return (u16)(u + a);
}
static void on_vblank(void) {
  ticks++;
  nmi_acc = mix(nmi_acc, ticks);
}
int main(void) {
  u16 i, a = 1, b = 2, expect_a = 1, expect_b = 2;
  nmiSet(on_vblank);
  *LT_ADDR(volatile u8, 0x4200) = 0x80;          /* NMI on */
  while (ticks < 20) {
    for (i = 0; i < 50; i++) {
      u16 na = mix(a, b), nb = mix(b, i);
      a = na; b = nb;
    }
    for (i = 0; i < 50; i++) {
      u16 na = (u16)(expect_a * 3u + expect_b), nb;
      na = (u16)((na ^ (na >> 3)) + expect_a);
      nb = (u16)(expect_b * 3u + i);
      nb = (u16)((nb ^ (nb >> 3)) + expect_b);
      expect_a = na; expect_b = nb;
    }
    CHECK(a == expect_a && b == expect_b);
  }
  *LT_ADDR(volatile u8, 0x4200) = 0x00;
  CHECK(nmi_acc != 0);
  return 0;
}
