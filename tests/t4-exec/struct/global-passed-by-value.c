// loomcc-do: run
// loomcc-int: agnostic
// A global struct passed by value: the callee's writes to its parameter must
// not reach the global (the same for a struct returned by value and then
// discarded). Found by a Csmith campaign (seed 73).
#include "loomcc-test.h"
struct S0 { const u8 f0; u8 f1; };
static struct S0 g = { 0x9f, 0xc0 };
static struct S0 bump(struct S0 p) {
  struct S0 r = { 0, 0xcb };
  for (p.f1 = 0; p.f1 <= 1; p.f1 = (u8)(p.f1 + 8)) ;
  return r;
}
static u8 read_f1(struct S0 p) { return p.f1; }
int main(void) {
  struct S0 local = { 1, 2 };
  bump(g);
  CHECK(g.f0 == 0x9f && g.f1 == 0xc0);
  (void)bump(local);
  CHECK(local.f1 == 2);
  CHECK(read_f1(g) == 0xc0);
  CHECK(bump(g).f1 == 0xcb && g.f1 == 0xc0);
  return 0;
}
