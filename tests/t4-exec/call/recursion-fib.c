// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static u16 fib(u16 n) { return n < 2 ? n : (u16)(fib((u16)(n - 1)) + fib((u16)(n - 2))); }
int main(void) {
  CHECK(fib(0) == 0 && fib(1) == 1 && fib(10) == 55 && fib(15) == 610);
  return 0;
}
