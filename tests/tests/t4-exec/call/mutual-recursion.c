// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static int is_odd(u16 n);
static int is_even(u16 n) { return n == 0 ? 1 : is_odd((u16)(n - 1)); }
static int is_odd(u16 n) { return n == 0 ? 0 : is_even((u16)(n - 1)); }
int main(void) {
  CHECK(is_even(10) && !is_even(7) && is_odd(33) && !is_odd(40));
  return 0;
}
