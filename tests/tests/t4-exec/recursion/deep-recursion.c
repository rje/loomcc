// loomcc-do: run
// loomcc-int: agnostic
// Recursion about 90 levels deep with a 24-byte local array per level:
// several kilobytes of hardware stack, near Loom's 7,632-byte budget.
#include "loomcc-test.h"
static u16 levels;
static u16 walk(u16 n) {
  u8 pad[24];
  u16 i, s = 0;
  for (i = 0; i < 24; i++) pad[i] = (u8)(n + i);
  if (n > 0) s = walk((u16)(n - 1));
  levels++;
  for (i = 0; i < 24; i++) s = (u16)(s + pad[i]);
  return s;
}
int main(void) {
  u16 expect = 0, n, i;
  for (n = 0; n <= 90; n++)
    for (i = 0; i < 24; i++) expect = (u16)(expect + (u8)(n + i));
  CHECK(walk(90) == expect);
  CHECK(levels == 91);
  return 0;
}
