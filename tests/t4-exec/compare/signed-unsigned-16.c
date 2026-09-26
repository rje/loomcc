// loomcc-do: run
// loomcc-int: 16
// Comparisons with a 16-bit unsigned int operand are unsigned.
#include "loomcc-test.h"
int main(void) {
  volatile int m = -1;
  volatile unsigned one = 1;
  volatile u16 us = 1;
  CHECK(m > one);           /* -1 converts to 65535 */
  CHECK(!(m < one));
  CHECK(m > us);            /* u16 promotes to unsigned int with 16-bit int */
  CHECK(m == 65535u);
  return 0;
}
