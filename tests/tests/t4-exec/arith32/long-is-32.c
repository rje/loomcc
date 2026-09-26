// loomcc-do: run
// loomcc-int: 16
// loomcc-note: loomcc's long is 32 bits (loomcc PLAN section 4); 816-tcc's is 16.
// loomcc-ref-diverges: tcc-rom [tcc-long16] 816-tcc's long is 16 bits
#include "loomcc-test.h"
int main(void) {
  volatile long a = 70000L;
  volatile unsigned long b = 0xffffffffUL;
  CHECK(sizeof(long) == 4);
  CHECK(a * 2 == 140000L);
  CHECK(b + 1 == 0);
  CHECK(a / 7 == 10000);
  CHECK(-1L < 1u);           /* unsigned int converts to long */
  return 0;
}
