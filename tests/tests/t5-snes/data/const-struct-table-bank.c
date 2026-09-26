// loomcc-do: run
// loomcc-int: 16
// loomcc-skip-mode: host16
// loomcc-extra-sources: aux/table0.c aux/table1.c aux/table2.c
// A pointer to a byte in a ROM table in another bank, stepped with ++ and
// indexed backwards: the bank byte must be carried in every access.
#include "loomcc-test.h"
extern const u8 t0[20000u], t1[20000u], t2[20000u];
static u16 sum_range(const u8 *p, u16 n) { u16 s = 0; while (n--) s = (u16)(s + *p++); return s; }
int main(void) {
  const u8 *end2 = t2 + 20000u;
  CHECK(sum_range(t1, 1) == 20);
  CHECK(sum_range(t2 + 19990u, 10) == 31);
  CHECK(end2[-1] == 31 && *(end2 - 20000u) == 30);
  CHECK(sum_range(t0 + 19999u, 1) == 11);
  return 0;
}
