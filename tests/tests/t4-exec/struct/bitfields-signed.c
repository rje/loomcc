// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
struct S { signed a : 4; signed b : 4; unsigned c : 4; };
int main(void) {
  struct S s;
  s.a = -1; s.b = 7; s.c = 15;
  CHECK(s.a == -1);
  CHECK(s.b == 7);
  s.b = s.b + 1;            /* 8 does not fit: implementation-defined, wraps to -8 everywhere here */
  CHECK(s.b == -8);
  CHECK(s.c == 15);
  CHECK(s.a + s.c == 14);
  return 0;
}
