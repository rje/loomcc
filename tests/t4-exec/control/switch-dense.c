// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static int f(int x) {
  switch (x) {
  case 0: return 10;
  case 1: return 11;
  case 2: return 12;
  case 3: return 13;
  case 4: return 14;
  case 5: return 15;
  case 6: return 16;
  case 7: return 17;
  default: return -1;
  }
}
int main(void) {
  int i;
  for (i = 0; i < 8; i++) CHECK(f(i) == 10 + i);
  CHECK(f(-1) == -1 && f(8) == -1 && f(1000) == -1 && f(-32768) == -1);
  return 0;
}
