// loomcc-do: run
// loomcc-int: 16
// loomcc-asm-sources: aux/call-c.asm
// loomcc-ref: tcc-rom
// loomcc-skip-mode: ir
// Hand-written assembly calls back into C through the 816-tcc ABI (as Loom's
// body.asm calls movement.c shims): the C functions must have ABI entries,
// and the caller's locals must survive the round trip through the assembly.
#include "loomcc-test.h"
i16 c_square(i16 x) { return (i16)(x * x); }
typedef struct { i16 x, y; u8 hits; } Probe;
void c_touch(Probe *p, i16 dx) { p->x = (i16)(p->x + dx); p->hits++; }
i16 asm_sum_squares(i16 a, i16 b);          /* c_square(a) + c_square(b) via jsl */
void asm_touch_twice(Probe *p, i16 dx);     /* c_touch(p, dx) twice via jsl */
int main(void) {
  i16 keep1 = 1234, keep2 = -77;
  Probe pr;
  pr.x = 10; pr.y = 20; pr.hits = 0;
  CHECK(asm_sum_squares(3, -4) == 25);
  CHECK(asm_sum_squares(100, 5) == 10025);
  asm_touch_twice(&pr, -7);
  CHECK(pr.x == -4 && pr.y == 20 && pr.hits == 2);
  CHECK(keep1 == 1234 && keep2 == -77);
  return 0;
}
