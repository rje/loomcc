// loomcc-do: run
// loomcc-int: agnostic
// A struct bigger than any register: copies and returns move every byte.
#include "loomcc-test.h"
struct Big { u8 b[37]; i16 tail; };
static struct Big make(u8 seed) { struct Big r; int i; for (i = 0; i < 37; i++) r.b[i] = (u8)(seed + i); r.tail = -seed; return r; }
static u16 sum(struct Big v) { u16 s = 0; int i; for (i = 0; i < 37; i++) s += v.b[i]; return (u16)(s + v.tail); }
int main(void) {
  struct Big a = make(10), b;
  b = a;
  a.b[0] = 0;
  CHECK(b.b[0] == 10 && b.b[36] == 46 && b.tail == -10);
  CHECK(sum(b) == (u16)(37 * 10 + 666 - 10));
  return 0;
}
