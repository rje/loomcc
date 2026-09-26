// loomcc-do: run
// loomcc-int: agnostic
// Pad state: pressed = now & ~before, released = before & ~now, held counters.
#include "loomcc-test.h"
static const u16 frames[8] = { 0x0000, 0x8000, 0x8080, 0x0080, 0x0000, 0x1234, 0x1234, 0xffff };
int main(void) {
  u16 prev = 0, pressed_total = 0, released_total = 0;
  u8 held_b = 0, i;
  for (i = 0; i < 8; i++) {
    u16 now = frames[i];
    u16 pressed = (u16)(now & ~prev);
    u16 released = (u16)(prev & ~now);
    pressed_total |= pressed;
    released_total |= released;
    held_b = (now & 0x8000) ? (u8)(held_b + 1) : 0;
    if (i == 2) CHECK(pressed == 0x0080 && released == 0 && held_b == 2);
    if (i == 3) CHECK(pressed == 0 && released == 0x8000 && held_b == 0);
    if (i == 7) CHECK(pressed == (u16)(0xffff & ~0x1234) && released == 0);
    prev = now;
  }
  CHECK(pressed_total == 0xffff);
  CHECK(released_total == 0x8080);
  CHECK(held_b == 1);
  return 0;
}
