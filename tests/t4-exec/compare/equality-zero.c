// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static int is0(i16 x) { return x == 0; }
static int nz(u16 x) { return x != 0; }
int main(void) {
  CHECK(is0(0) && !is0(1) && !is0(-1) && !is0(256));
  CHECK(!nz(0) && nz(1) && nz(0x8000) && nz(0x100));
  return 0;
}
