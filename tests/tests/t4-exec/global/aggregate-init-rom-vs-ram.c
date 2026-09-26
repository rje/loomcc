// loomcc-do: run
// loomcc-int: agnostic
// const tables live in ROM; initialised non-const data is copied to RAM at startup.
#include "loomcc-test.h"
static const u16 rom_tab[4] = { 1, 2, 3, 4 };
static u16 ram_tab[4] = { 5, 6, 7, 8 };
struct Cfg { u8 speed; i16 limit; const u16 *tab; };
static struct Cfg cfg = { 3, -100, rom_tab };
int main(void) {
  CHECK(rom_tab[3] == 4);
  CHECK(ram_tab[0] == 5);
  ram_tab[0] = 50;
  CHECK(ram_tab[0] == 50);
  CHECK(cfg.speed == 3 && cfg.limit == -100 && cfg.tab[1] == 2);
  cfg.tab = ram_tab;
  CHECK(cfg.tab[0] == 50);
  return 0;
}
