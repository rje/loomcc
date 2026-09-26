// loomcc-do: run
// loomcc-int: agnostic
// Inside a switch, break leaves the switch; continue continues the loop.
#include "loomcc-test.h"
int main(void) {
  u8 i, a = 0, b = 0, c = 0;
  for (i = 0; i < 12; i++) {
    switch (i % 3) {
    case 0: a++; break;
    case 1: continue;
    default: b++;
    }
    c++;
  }
  CHECK(a == 4 && b == 4 && c == 8);
  return 0;
}
