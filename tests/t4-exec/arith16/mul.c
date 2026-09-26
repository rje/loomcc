// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static i16 mul(i16 a, i16 b) { return (i16)(a * b); }
static u16 umul(u16 a, u16 b) { return (u16)(1u * a * b); }   /* no signed overflow on 32-bit-int hosts */
int main(void) {
  CHECK(mul(3, 4) == 12);
  CHECK(mul(-3, 4) == -12);
  CHECK(mul(-3, -4) == 12);
  CHECK(mul(0, 12345) == 0);
  CHECK(mul(1, -32768) == -32768);
  CHECK(mul(181, 181) == 32761);
  CHECK(umul(300, 300) == 24464u);
  CHECK(umul(256, 256) == 0);
  CHECK(umul(255, 257) == 65535u);
  CHECK(umul(65535u, 65535u) == 1);
  return 0;
}
