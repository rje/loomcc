// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static u16 gcd(u16 a, u16 b) { while (b) { u16 t = (u16)(a % b); a = b; b = t; } return a; }
int main(void) {
  CHECK(gcd(48, 18) == 6 && gcd(17, 5) == 1 && gcd(0, 9) == 9 && gcd(65535u, 255) == 255);
  CHECK((u32)48 * 18 / gcd(48, 18) == 144);
  return 0;
}
