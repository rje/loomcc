// loomcc-do: run
// loomcc-int: 16
// loomcc-tcc-sources: aux/tcc-chain.c
// unit -> 816-tcc -> unit -> 816-tcc -> unit: locals in each unit frame must
// survive the nested calls (loomcc's static frames must not be shared by
// activations that are live at the same time).
#include "loomcc-test.h"
i16 tcc_step(i16 depth, i16 acc);
i16 unit_step(i16 depth, i16 acc) {
  i16 mine = (i16)(depth * 10 + 1);
  i16 inner = depth > 0 ? tcc_step((i16)(depth - 1), (i16)(acc + mine)) : acc;
  return (i16)(inner + mine);
}
int main(void) {
  /* depth 4 unit, 3 tcc, 2 unit, 1 tcc, 0 unit */
  CHECK(unit_step(4, 0) == 213);
  return 0;
}
