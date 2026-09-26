// loomcc-do: run
// loomcc-int: 16
// loomcc-skip-mode: host16
// loomcc-extra-sources: aux/table0.c aux/table1.c aux/table2.c
// Three 20000-byte const tables, one per unit (each compiler puts a unit's
// const data in one section), cannot share a 32 KiB LoROM bank: at least two
// live in banks other than the code's, so every read needs the bank byte.
// host16 is skipped: 60000 bytes do not fit msp430's 64 KiB address space.
#include "loomcc-test.h"
#define K 20000u
extern const u8 t0[K], t1[K], t2[K];
static const u8 *const tables[3] = { t0, t1, t2 };
static u16 last(const u8 *t) { return t[K - 1]; }
int main(void) {
  volatile u16 i = K - 1;
  u16 n;
  CHECK(t0[0] == 10 && t1[0] == 20 && t2[0] == 30);
  CHECK(t0[i] == 11 && t1[i] == 21 && t2[i] == 31);
  for (n = 0; n < 3; n++) CHECK(last(tables[n]) == 10 * (n + 1) + 1);
  CHECK(t1[K / 2] == 0);
  return 0;
}
