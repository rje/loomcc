// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
struct S { u8 pad; i16 arr[4]; };
int main(void) {
  struct S s;
  i16 *p = s.arr;
  int i;
  for (i = 0; i < 4; i++) p[i] = (i16)(i * i);
  CHECK(s.arr[3] == 9);
  CHECK(&s.arr[2] - p == 2);
  return 0;
}
