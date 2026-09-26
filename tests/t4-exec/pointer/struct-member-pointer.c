// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
struct S { u8 a; i16 b; u8 c; };
int main(void) {
  struct S s;
  i16 *pb = &s.b;
  u8 *pc = &s.c;
  *pb = -300;
  *pc = 9;
  CHECK(s.b == -300 && s.c == 9);
  return 0;
}
