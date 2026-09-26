// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
static const u16 tbl[6] = { 5, 10, 15, 20, 25, 30 };
int main(void) {
  const u16 *p, *end = tbl + 6;
  u16 s = 0;
  for (p = tbl; p < end; p += 2) s = (u16)(s + *p);
  CHECK(s == 45);
  CHECK(end - tbl == 6 && end > tbl && !(end <= tbl));
  return 0;
}
