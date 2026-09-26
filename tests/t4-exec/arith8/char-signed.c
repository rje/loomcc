// loomcc-do: run
// loomcc-int: agnostic
// loomcc-note: plain char is signed on every target here (implementation-defined).
#include "loomcc-test.h"
int main(void) {
  volatile char c = (char)0xff;
  CHECK(c == -1);
  CHECK(c < 0);
  volatile char d = 'A';
  CHECK(d + 1 == 'B');
  return 0;
}
