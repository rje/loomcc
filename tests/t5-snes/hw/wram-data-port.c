// loomcc-do: run
// loomcc-int: 16
// loomcc-ref: tcc-rom
// loomcc-skip-mode: ir
// Volatile hardware registers: the WRAM data port ($2180 with its address in
// $2181-$2183) auto-increments on every access, so each volatile write must
// happen exactly once and in program order.
#include "loomcc-test.h"
#define WMDATA LT_ADDR(volatile u8, 0x2180)
#define WMADDL LT_ADDR(volatile u8, 0x2181)
#define WMADDM LT_ADDR(volatile u8, 0x2182)
#define WMADDH LT_ADDR(volatile u8, 0x2183)
static u8 target[8];
int main(void) {
  u32 addr = (u32)(u16)&target[0] | 0x7e0000ul;   /* target lives in bank $7E (.bss) */
  u8 i;
  *WMADDL = (u8)addr;
  *WMADDM = (u8)(addr >> 8);
  *WMADDH = (u8)((addr >> 16) & 1);
  for (i = 0; i < 8; i++) *WMDATA = (u8)(0x40 + i);
  *WMDATA = 0x99;                 /* one more: lands after the array */
  for (i = 0; i < 8; i++) CHECK(target[i] == 0x40 + i);
  return 0;
}
