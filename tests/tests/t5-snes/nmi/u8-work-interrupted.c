// loomcc-do: run
// loomcc-int: 16
// loomcc-ref: tcc-rom
// loomcc-skip-mode: ir
// The main loop runs long stretches of 8-bit work (where loomcc keeps the
// accumulator in 8-bit mode) while VBlank interrupts run C that does its own
// 8- and 16-bit work: every register, flag and scratch word the interrupted
// code relies on must come back intact.
#include "loomcc-test.h"
void nmiSet(void (*vblankRoutine)(void));
static volatile u16 ticks;
static u8 nmi_bytes[16];
static void on_vblank(void) {
  u8 i;
  u16 w = ticks;
  for (i = 0; i < 16; i++) nmi_bytes[i] = (u8)(nmi_bytes[i] + i + (u8)w);
  ticks = (u16)(w + 1);
}
static u8 crc8(const u8 *p, u8 n) {
  u8 c = 0xff, i;
  while (n--) { c ^= *p++; for (i = 0; i < 8; i++) c = (u8)((c & 0x80) ? (c << 1) ^ 0x31 : c << 1); }
  return c;
}
int main(void) {
  static u8 data[64];
  u8 i, first, again;
  u16 rounds = 0;
  for (i = 0; i < 64; i++) data[i] = (u8)(i * 7 + 3);
  first = crc8(data, 64);
  nmiSet(on_vblank);
  *LT_ADDR(volatile u8, 0x4200) = 0x80;
  while (ticks < 30) {
    again = crc8(data, 64);
    CHECK(again == first);
    rounds++;
  }
  *LT_ADDR(volatile u8, 0x4200) = 0x00;
  CHECK(rounds > 3);
  return 0;
}
