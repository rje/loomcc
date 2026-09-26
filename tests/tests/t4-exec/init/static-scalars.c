// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static i16 a = -5;
static u16 b = 65535u;
static u8 c = 200;
static i32 d = -100000;
static i16 zero;
int main(void) {
  CHECK(a == -5 && b == 65535u && c == 200 && d == -100000 && zero == 0);
  a = 7;
  CHECK(a == 7);
  return 0;
}
