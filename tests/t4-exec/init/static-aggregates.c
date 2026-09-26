// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
struct P { i16 x; u8 y; i16 z[2]; };
static struct P p = { -1, 2, { 3, -4 } };
static struct P arr[3] = { { 1 }, { 2, 3 }, { 4, 5, { 6, 7 } } };
static i16 partial[5] = { 1, 2 };
int main(void) {
  CHECK(p.x == -1 && p.y == 2 && p.z[1] == -4);
  CHECK(arr[0].x == 1 && arr[0].y == 0 && arr[0].z[1] == 0);
  CHECK(arr[1].y == 3 && arr[2].z[1] == 7);
  CHECK(partial[1] == 2 && partial[4] == 0);
  return 0;
}
