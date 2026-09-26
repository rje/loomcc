// loomcc-do: run
// loomcc-int: agnostic
// A 300-byte struct passed by value through a call that cannot be inlined:
// the callee reads its last member 300 bytes up the stack, beyond the reach
// of an 8-bit stack-relative offset (found through tcc tests2
// 130_large_argument).
#include "loomcc-test.h"
struct big { u16 a[150]; };
static u16 last(struct big b) { return (u16)(b.a[0] + b.a[149] * 3u + b.a[100]); }
u16 (*volatile fp)(struct big) = last;
int main(void) {
  struct big b;
  u16 i;
  for (i = 0; i < 150; i++) b.a[i] = (u16)(i * 7u + 1u);
  CHECK(fp(b) == 1u + (149u * 7u + 1u) * 3u + 701u);
  return 0;
}
