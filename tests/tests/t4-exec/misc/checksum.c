// loomcc-do: run
// loomcc-int: agnostic
// A Fletcher-16 checksum: mixed 8/16-bit arithmetic in a loop.
#include "loomcc-test.h"
static u16 fletcher16(const u8 *d, u16 n) {
  u16 s1 = 0, s2 = 0;
  while (n--) { s1 = (u16)((s1 + *d++) % 255); s2 = (u16)((s2 + s1) % 255); }
  return (u16)((s2 << 8) | s1);
}
int main(void) {
  static const u8 msg[] = { 'a', 'b', 'c', 'd', 'e' };
  static const u8 msg2[] = { 'a', 'b', 'c', 'd', 'e', 'f' };
  CHECK(fletcher16(msg, 5) == 0xc8f0);
  CHECK(fletcher16(msg2, 6) == 0x2057);
  return 0;
}
