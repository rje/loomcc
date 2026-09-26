// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static void fill(void *dst, u8 v, u16 n) { u8 *d = (u8 *)dst; while (n--) *d++ = v; }
int main(void) {
  u16 w[4];
  fill(w, 0xab, sizeof w);
  CHECK(w[0] == 0xabab && w[3] == 0xabab);
  return 0;
}
