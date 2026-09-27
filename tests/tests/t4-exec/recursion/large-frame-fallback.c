// loomcc-do: run
// loomcc-int: agnostic
// A recursive function whose frame is too big for direct-page addressing
// (a 300-byte local) keeps a static frame, saved around its recursive
// calls; its values still survive each level.
#include "loomcc-test.h"
static u16 big(u8 n) {
  u8 buf[300];
  u16 i, s = 0;
  for (i = 0; i < 300; i++) buf[i] = (u8)(n + i);
  if (n > 0) s = big((u8)(n - 1));
  for (i = 0; i < 300; i += 50) s = (u16)(s + buf[i]);
  return s;
}
int main(void) {
  u16 expect = 0, i;
  u8 n;
  for (n = 0; n <= 4; n++)
    for (i = 0; i < 300; i += 50) expect = (u16)(expect + (u8)(n + i));
  CHECK(big(4) == expect);
  return 0;
}
