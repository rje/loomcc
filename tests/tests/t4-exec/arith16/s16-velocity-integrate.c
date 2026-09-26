// loomcc-do: run
// loomcc-int: agnostic
// s16 velocity integration with friction toward zero, both signs.
#include "loomcc-test.h"
static i16 friction(i16 v, i16 f) { if (v > 0) return v > f ? (i16)(v - f) : 0; if (v < 0) return v < -f ? (i16)(v + f) : 0; return 0; }
int main(void) {
  i16 v = 100, w = -100, x = 0;
  u8 t;
  for (t = 0; t < 20; t++) { x = (i16)(x + v); v = friction(v, 7); w = friction(w, 7); }
  CHECK(v == 0 && w == 0);
  CHECK(x == 100 + 93 + 86 + 79 + 72 + 65 + 58 + 51 + 44 + 37 + 30 + 23 + 16 + 9 + 2);
  return 0;
}
