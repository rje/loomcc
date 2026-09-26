// loomcc-do: run
// loomcc-int: 16
// -1 / 2u: -1 converts to 65535 (16-bit unsigned int).
#include "loomcc-test.h"
int main(void) {
  volatile int m = -1;
  volatile unsigned two = 2;
  CHECK(m / two == 32767u);
  CHECK(m % 7u == 65535u % 7u);
  CHECK((unsigned)m >> 1 == 32767u);
  return 0;
}
