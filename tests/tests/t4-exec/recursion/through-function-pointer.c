// loomcc-do: run
// loomcc-int: agnostic
// Recursion through a function pointer: the call goes through the 816-tcc
// ABI entry of an address-taken recursive function, with a struct result
// and an address-taken local per level.
#include "loomcc-test.h"
typedef struct { i16 sum; i16 count; } Pair;
typedef Pair (*Step)(i16 n, i16 *seen);
static Step step_ptr;
static Pair step(i16 n, i16 *seen) {
  Pair p;
  i16 here = n;
  *seen += 1;
  if (n == 0) { p.sum = 0; p.count = 0; return p; }
  p = step_ptr((i16)(n - 1), &here);
  CHECK(here == n + 1);
  p.sum = (i16)(p.sum + n);
  p.count = (i16)(p.count + 1);
  return p;
}
int main(void) {
  i16 seen = 0;
  Pair r;
  step_ptr = step;
  r = step_ptr(10, &seen);
  CHECK(r.sum == 55 && r.count == 10 && seen == 1);
  return 0;
}
