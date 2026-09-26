// loomcc-do: run
// loomcc-int: 16
// loomcc-asm-sources: aux/u8-args.asm
// loomcc-ref: tcc-rom
// loomcc-skip-mode: ir
// 8-bit arguments are one-byte stack slots in the 816-tcc ABI; hand-written
// assembly reads them at those offsets. A function pointer to assembly is
// called too (through 816-tcc's tcc__jsl_r10 convention or loomcc's own).
#include "loomcc-test.h"
u16 asm_pack(u8 a, u8 b, u16 c, u8 d);   /* a | b << 4 | ... see the asm */
int main(void) {
  u16 (*fp)(u8, u8, u16, u8) = asm_pack;
  CHECK(asm_pack(1, 2, 0x100, 3) == (u16)(1 + 2 * 16 + 0x100 + 3 * 0x1000));
  CHECK(fp(0xf, 0xf, 0, 0xf) == (u16)(15 + 15 * 16 + 15 * 0x1000));
  return 0;
}
