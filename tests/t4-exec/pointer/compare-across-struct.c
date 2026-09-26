// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
struct S { u8 a; i16 b; u8 c[3]; };
int main(void) {
  struct S s;
  CHECK((u8 *)&s.a < (u8 *)&s.b);
  CHECK((u8 *)&s.b < &s.c[0]);
  CHECK(&s.c[2] - &s.c[0] == 2);
  return 0;
}
