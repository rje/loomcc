// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static i16 add(i16 a, i16 b) { return (i16)(a + b); }
static i16 sub(i16 a, i16 b) { return (i16)(a - b); }
static i16 mul(i16 a, i16 b) { return (i16)(a * b); }
static i16 (*const ops[])(i16, i16) = { add, sub, mul };
static i16 apply(i16 (*f)(i16, i16), i16 a, i16 b) { return f(a, b); }
int main(void) {
  i16 (*f)(i16, i16) = sub;
  CHECK(f(10, 3) == 7);
  CHECK(ops[0](2, 3) == 5 && ops[1](2, 3) == -1 && ops[2](2, 3) == 6);
  CHECK(apply(mul, -4, 5) == -20);
  f = 0;
  CHECK(f == 0);
  return 0;
}
