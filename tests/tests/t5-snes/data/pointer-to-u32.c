// loomcc-do: run
// loomcc-int: 16
// loomcc-ref: tcc-rom
// loomcc-skip-mode: ir
// Converting a far pointer to a 32-bit integer keeps all 24 address bits
// (implementation-defined, but the only useful choice on the 65816: DMA
// source registers need the bank).
// loomcc-ref-diverges: tcc-rom [tcc-ptr-to-int32] 816-tcc converts a pointer to a 32-bit integer by sign-extending its low word, losing the bank
#include "loomcc-test.h"
static const u8 rom_table[4] = { 1, 2, 3, 4 };
static u8 ram_table[4];
int main(void) {
  u32 r = (u32)&rom_table[0], w = (u32)&ram_table[0];
  CHECK((r >> 16) != 0 && (r >> 16) != 0xffff);           /* a ROM bank, $00-$7D or $80-$FF */
  CHECK((w >> 16) == 0x7e);                              /* .bss is in bank $7E */
  CHECK((u16)w == (u16)(u32)&ram_table[0]);
  CHECK(*(const u8 *)(u32)(r + 2) == 3);
  return 0;
}
