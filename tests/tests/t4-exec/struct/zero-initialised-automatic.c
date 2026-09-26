// loomcc-do: run
// loomcc-int: agnostic
// A partially initialised automatic aggregate zero-fills the rest.
#include "loomcc-test.h"
typedef struct { u8 a; i16 b[4]; u16 c; } S;
static u16 check(u8 seed) {
  S s = { seed };
  u8 arr[10] = { 1 };
  return (u16)(s.a + s.b[0] + s.b[3] + s.c + arr[0] + arr[9]);
}
int main(void) {
  volatile u8 junk[40];
  u8 i;
  for (i = 0; i < 40; i++) junk[i] = 0xee;
  CHECK(check(5) == 6);
  return 0;
}
