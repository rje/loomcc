// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static int f(i16 x) {
  switch (x) {
  case -32768: return 1;
  case -1: return 2;
  case 0: return 3;
  case 100: return 4;
  case 1000: return 5;
  case 32767: return 6;
  }
  return 0;
}
int main(void) {
  CHECK(f(-32768) == 1 && f(-1) == 2 && f(0) == 3 && f(100) == 4 && f(1000) == 5 && f(32767) == 6);
  CHECK(f(1) == 0 && f(-2) == 0 && f(999) == 0);
  return 0;
}
