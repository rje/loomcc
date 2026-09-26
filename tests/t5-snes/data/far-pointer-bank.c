// loomcc-do: run
// loomcc-int: 16
// Pointers carry a bank: a pointer to WRAM ($7E) and one to ROM data (a
// $80+ or $00-$3F bank) must both dereference correctly through one function.
#include "loomcc-test.h"
static const u16 rom_data[2] = { 0x1111, 0x2222 };
static u16 ram_data[2] = { 0x3333, 0x4444 };
static u16 read2(const u16 *p) { return p[1]; }
int main(void) {
  CHECK(read2(rom_data) == 0x2222);
  CHECK(read2(ram_data) == 0x4444);
  ram_data[1] = 0x5555;
  CHECK(read2(ram_data) == 0x5555);
  return 0;
}
