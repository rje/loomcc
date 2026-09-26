// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static int f(int x) {
  int r = 0;
  switch (x) {
  default: r += 1000;
  case 1: r += 1;
  case 2: r += 10; break;
  case 3: r += 100;
  }
  return r;
}
int main(void) {
  CHECK(f(1) == 11);
  CHECK(f(2) == 10);
  CHECK(f(3) == 100);
  CHECK(f(9) == 1011);
  return 0;
}
