// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
struct In { u8 a; i16 b; };
struct Out { struct In in[2]; u16 n; struct In last; };
int main(void) {
  struct Out o;
  o.in[0].a = 1; o.in[0].b = -1;
  o.in[1].a = 2; o.in[1].b = -2;
  o.n = 2;
  o.last = o.in[1];
  CHECK(o.last.a == 2 && o.last.b == -2);
  CHECK(o.in[0].b + o.in[1].b == -3);
  return 0;
}
