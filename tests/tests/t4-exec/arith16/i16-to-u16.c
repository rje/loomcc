// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
int main(void) {
  volatile i16 a = -1, b = -32768;
  CHECK((u16)a == 65535u);
  CHECK((u16)b == 32768u);
  volatile u16 c = 40000;
  CHECK((i16)c == -25536);
  return 0;
}
