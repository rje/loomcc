// loomcc-do: run
// loomcc-int: agnostic
// Long runs of 8-bit work between 16-bit operations and calls: values must
// survive every sep/rep boundary (the M flag at call boundaries is 16-bit).
#include "loomcc-test.h"
static u8 mix(u8 a, u8 b) { return (u8)((a ^ b) + (a & b) * 2); }
static u16 widen(u8 a, u8 b) { return (u16)((a << 8) | b); }
int main(void) {
  u8 acc = 0;
  u16 w = 0;
  u8 i;
  for (i = 0; i < 200; i++) {
    acc = mix(acc, i);
    w += widen(acc, i);
  }
  CHECK(acc == (u8)19900);         /* mix(a, b) == a + b modulo 256 */
  CHECK(w == 0x81bc);
  return 0;
}
