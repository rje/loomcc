// loomcc-do: run
// loomcc-int: 16
// loomcc-tcc-sources: aux/tcc-structs.c
// Structs cross the ABI by value both ways: copied whole as arguments,
// returned through the hidden first-argument pointer.
#include "loomcc-test.h"
struct V { i16 x, y; u8 tag; };
struct V tcc_make(i16 x, i16 y, u8 tag);
i16 tcc_sum(struct V v);
struct V unit_swap(struct V v) { struct V r; r.x = v.y; r.y = v.x; r.tag = (u8)(v.tag + 1); return r; }
i16 tcc_use_unit(void);
int main(void) {
  struct V v = tcc_make(3, -4, 9);
  CHECK(v.x == 3 && v.y == -4 && v.tag == 9);
  CHECK(tcc_sum(v) == 8);
  CHECK(tcc_use_unit() == 1);
  return 0;
}
