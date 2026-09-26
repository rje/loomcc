// loomcc-do: run
// loomcc-int: agnostic
// Case labels inside nested blocks and loops still belong to the switch.
#include "loomcc-test.h"
static int f(int x) {
  int r = 0;
  switch (x) {
  case 0:
    r = 1;
    if (r) {
  case 1:
      r += 10;
    }
    break;
  }
  return r;
}
int main(void) {
  CHECK(f(0) == 11);
  CHECK(f(1) == 10);
  CHECK(f(2) == 0);
  return 0;
}
