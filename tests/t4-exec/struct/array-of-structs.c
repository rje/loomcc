// loomcc-do: run
// loomcc-int: agnostic
// a[i].f with a non-power-of-two element size.
#include "loomcc-test.h"
struct E { u8 kind; i16 x; i16 y; u8 flags; };
static struct E ents[10];
int main(void) {
  int i;
  i16 sum = 0;
  for (i = 0; i < 10; i++) { ents[i].kind = (u8)i; ents[i].x = (i16)(i * 10); ents[i].y = (i16)-i; ents[i].flags = (u8)(i & 1); }
  for (i = 0; i < 10; i++) sum += ents[i].x + ents[i].y + ents[i].flags;
  CHECK(sum == 450 - 45 + 5);
  CHECK(ents[7].kind == 7 && ents[7].x == 70);
  return 0;
}
