// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static i16 data[4] = { 10, 20, 30, 40 };
static i16 *p = &data[2];
static i16 *q = data + 3;
static struct { i16 *ptr; u8 n; } desc = { data, 4 };
int main(void) {
  CHECK(*p == 30 && *q == 40);
  CHECK(desc.ptr[1] == 20 && desc.n == 4);
  return 0;
}
