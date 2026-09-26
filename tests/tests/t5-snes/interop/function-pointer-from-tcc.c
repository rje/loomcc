// loomcc-do: run
// loomcc-int: 16
// loomcc-tcc-sources: aux/tcc-fnptr.c
// A function pointer made by 816-tcc code, called by the unit, and a unit
// function pointer stored in an 816-tcc table and called back by it.
#include "loomcc-test.h"
typedef i16 (*op_fn)(i16, i16);
op_fn tcc_get_op(u8 which);
void tcc_register(op_fn f);
i16 tcc_run_registered(i16 a, i16 b);
static i16 unit_mul(i16 a, i16 b) { return (i16)(a * b); }
int main(void) {
  op_fn f = tcc_get_op(0), g = tcc_get_op(1);
  CHECK(f(7, 5) == 12 && g(7, 5) == 2);
  tcc_register(unit_mul);
  CHECK(tcc_run_registered(-6, 7) == -42);
  return 0;
}
