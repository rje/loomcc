// loomcc-do: run
// loomcc-ref-diverges: tcc-rom [tcc-ptr-to-int32] 816-tcc converts a pointer to a 32-bit integer by sign-extending its low word, losing the bank
// loomcc-int: 16
// loomcc-ref: tcc-rom
// loomcc-skip-mode: ir
// A DMA set up from C: a const ROM table copied into a WRAM buffer through
// the WRAM data port. The table's 24-bit address must be split into the
// DMA source registers correctly (low, high, bank).
#include "loomcc-test.h"
#define REG8(a) LT_ADDR(volatile u8, a)
static const u8 src[32] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                            17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 };
static u8 dst[32];
int main(void) {
  const u8 *s = src;
  u32 sa = (u32)s;
  u8 i;
  u16 da = (u16)&dst[0];
  *REG8(0x2181) = (u8)da;
  *REG8(0x2182) = (u8)(da >> 8);
  *REG8(0x2183) = 0;                      /* bank $7E */
  *REG8(0x4300) = 0x00;                   /* A->B, one register, increment */
  *REG8(0x4301) = 0x80;                   /* B bus: $2180 */
  *REG8(0x4302) = (u8)sa;
  *REG8(0x4303) = (u8)(sa >> 8);
  *REG8(0x4304) = (u8)(sa >> 16);
  *REG8(0x4305) = 32;
  *REG8(0x4306) = 0;
  *REG8(0x420b) = 0x01;                   /* start channel 0 */
  for (i = 0; i < 32; i++) CHECK(dst[i] == i + 1);
  return 0;
}
