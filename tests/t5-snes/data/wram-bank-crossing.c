// loomcc-do: run
// loomcc-int: 16
// loomcc-ref: tcc-rom
// loomcc-skip-mode: ir
// WRAM is one 128 KiB block across banks $7E and $7F: a pointer walking up
// from $7E:FFF8 must carry into the bank byte at $7F:0000 (++, [], +=,
// difference and comparison all use the full 24 bits). The bytes around the
// boundary are free in the harness ROM (no initialised data in this test).
// loomcc-note: No C object the toolchain lays out can span a bank (sections
// loomcc-note: never do), so a compiler may reasonably step pointers in 16
// loomcc-note: bits; this only matters for code that treats WRAM as one array
// loomcc-note: through absolute addresses. Design question Q3 in docs/FINDINGS.md.
// loomcc-xfail: design question Q3 (pointer steps across the $7E/$7F boundary)
// loomcc-ref-diverges: tcc-rom [tcc-bank-wrap] 816-tcc's stores through p++ wrap within bank $7E
#include "loomcc-test.h"
#define BASE LT_ADDR(u8, 0x7efff8)
int main(void) {
  u8 *p = BASE, *q;
  u8 i;
  u16 sum = 0;
  for (i = 0; i < 16; i++) *p++ = (u8)(i * 3 + 1);
  CHECK(p == LT_ADDR(u8, 0x7f0008));
  CHECK(p - BASE == 16);
  CHECK(*LT_ADDR(u8, 0x7f0000) == 8 * 3 + 1);
  q = BASE;
  for (i = 0; i < 16; i++) sum = (u16)(sum + q[i]);
  CHECK(sum == 16 + 3 * 120);
  q += 9;
  CHECK(*q == 9 * 3 + 1 && q > BASE + 7);
  CHECK(((u16 *)BASE)[4] == (u16)((8 * 3 + 1) | ((9 * 3 + 1) << 8)));
  return 0;
}
