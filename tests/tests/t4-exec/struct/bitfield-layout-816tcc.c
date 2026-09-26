// loomcc-do: run
// loomcc-int: 16
// loomcc-note: 816-tcc's bit-field allocation (LSB first, 16-bit units, no
// loomcc-note: straddling; loomcc PLAN section 9), observed through a union.
#include "loomcc-test.h"
struct B { unsigned a : 3, b : 5, c : 9; unsigned char d; };
union U { struct B s; u16 w[3]; };
int main(void) {
  union U u;
  u.w[0] = 0; u.w[1] = 0; u.w[2] = 0;
  u.s.a = 5; u.s.b = 17; u.s.c = 300; u.s.d = 0x7e;
  CHECK(u.w[0] == (5 | (17 << 3)));
  CHECK(u.w[1] == 300);
  CHECK((u.w[2] & 0xff) == 0x7e);
  CHECK(sizeof(struct B) == 6);
  return 0;
}
