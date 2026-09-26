// loomcc-do: run
// loomcc-int: agnostic
// Each function's static local is its own object, even with the same name.
#include "loomcc-test.h"
static u8 a(void) { static u8 n; return ++n; }
static u8 b(void) { static u8 n = 100; return ++n; }
int main(void) {
  a(); a(); b();
  CHECK(a() == 3 && b() == 102);
  return 0;
}
