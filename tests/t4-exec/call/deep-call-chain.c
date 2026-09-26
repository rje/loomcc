// loomcc-do: run
// loomcc-int: agnostic
// A chain of distinct functions (no recursion): each keeps a live local across its call.
#include "loomcc-test.h"
static i16 f6(i16 x) { return (i16)(x + 6); }
static i16 f5(i16 x) { i16 k = (i16)(x * 2); return (i16)(f6(x) + k); }
static i16 f4(i16 x) { i16 k = (i16)(x - 1); return (i16)(f5(x) + k); }
static i16 f3(i16 x) { i16 k = (i16)(x ^ 3); return (i16)(f4(x) + k); }
static i16 f2(i16 x) { i16 k = (i16)(x + 100); return (i16)(f3(x) + k); }
static i16 f1(i16 x) { i16 k = (i16)(-x); return (i16)(f2(x) + k); }
int main(void) {
  i16 x = 5;
  CHECK(f1(x) == (x + 6) + 2 * x + (x - 1) + (x ^ 3) + (x + 100) + (-x));
  return 0;
}
