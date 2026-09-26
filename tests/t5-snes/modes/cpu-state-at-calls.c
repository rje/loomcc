// loomcc-do: run
// loomcc-int: 16
// loomcc-asm-sources: aux/cpu-state.asm
// loomcc-ref: tcc-rom
// loomcc-skip-mode: ir
// At every call to foreign code the CPU must be in the 816-tcc ABI state:
// 16-bit A and X/Y (M and X flags clear), D = $0000, DBR = $7E, even right
// after long runs of 8-bit work. asm_cpu_state() reports any deviation.
#include "loomcc-test.h"
u16 asm_cpu_state(void);   /* bit 5: M set, bit 4: X set, bit 8: D != 0, bit 9: DBR != $7E */
static u8 bytes[32];
static u8 mix8(u8 a, u8 b) { return (u8)((a ^ b) + (a >> 1)); }
int main(void) {
  u8 i, acc = 0x5a;
  u16 bad = 0;
  for (i = 0; i < 32; i++) {
    acc = mix8(acc, i);
    bytes[i] = acc;
    bad |= asm_cpu_state();
    if (bytes[i] & 1) { acc = (u8)(acc - 3); bad |= asm_cpu_state(); }
  }
  bad |= asm_cpu_state();
  CHECK(bad == 0);
  CHECK(bytes[31] == acc);
  return 0;
}
