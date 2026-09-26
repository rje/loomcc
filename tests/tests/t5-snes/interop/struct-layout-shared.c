// loomcc-do: run
// loomcc-int: 16
// loomcc-tcc-sources: aux/tcc-layout.c
// 816-tcc code reads and writes a struct with pointer members that the unit
// under test laid out (pointers 4 bytes, 4-aligned: loomcc PLAN section 1).
#include "loomcc-test.h"
struct Obj { u8 kind; u8 *data; i16 x; void (*fn)(void); u8 last; };
u16 tcc_obj_size(void);
i16 tcc_obj_check(struct Obj *o);
static u8 payload[3] = { 7, 8, 9 };
static void noop(void) {}
int main(void) {
  struct Obj o;
  CHECK(sizeof(struct Obj) == tcc_obj_size());
  o.kind = 3; o.data = payload; o.x = -5; o.fn = noop; o.last = 0x5a;
  CHECK(tcc_obj_check(&o) == 1);
  CHECK(o.x == 6 && o.last == 0xa5);
  return 0;
}
