// loomcc-do: run
// loomcc-int: 16
// loomcc-skip-mode: host16
// loomcc-extra-sources: aux/table0.c aux/table1.c aux/table2.c
// A pointer to a ROM table in another bank, taken at run time (not in a
// static initialiser) and read through: the pointer must carry the bank.
#include "loomcc-test.h"
extern const u8 t0[20000u], t1[20000u], t2[20000u];
static const u8 *pick(u8 k) { const u8 *p; if (k == 0) p = t0; else if (k == 1) p = t1; else p = t2; return p; }
int main(void) {
  u8 k;
  for (k = 0; k < 3; k++) {
    const u8 *p = pick(k);
    CHECK(p[0] == 10 * (k + 1));
    CHECK(p[19999u] == 10 * (k + 1) + 1);
  }
  return 0;
}
