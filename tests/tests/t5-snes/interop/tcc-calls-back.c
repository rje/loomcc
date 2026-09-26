// loomcc-do: run
// loomcc-int: 16
// loomcc-tcc-sources: aux/tcc-caller.c
// 816-tcc code calls functions of the unit under test, directly and through
// a function pointer (816-tcc's tcc__jsl_r10).
#include "loomcc-test.h"
i16 tcc_call_direct(i16 x);
i16 tcc_call_pointer(i16 (*f)(i16, u8), i16 x);
i16 unit_twice(i16 x) { return (i16)(x * 2); }
static i16 unit_scale(i16 x, u8 by) { return (i16)(x * by); }
int main(void) {
  CHECK(tcc_call_direct(21) == 42 + 1);
  CHECK(tcc_call_pointer(unit_scale, -7) == -21);
  return 0;
}
