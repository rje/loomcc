// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
struct P { i16 x; u8 y; };
static i16 f(i16 v) {
  i16 arr[4] = { v, (i16)(v + 1) };
  struct P p = { (i16)(v * 2), 9 };
  return (i16)(arr[0] + arr[1] + arr[2] + arr[3] + p.x + p.y);
}
int main(void) {
  CHECK(f(10) == 10 + 11 + 20 + 9);
  CHECK(f(-1) == -1 + 0 - 2 + 9);
  return 0;
}
