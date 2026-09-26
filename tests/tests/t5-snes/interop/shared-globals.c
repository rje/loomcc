// loomcc-do: run
// loomcc-int: 16
// loomcc-tcc-sources: aux/tcc-globals.c
// Globals defined by one compiler, used by the other: .bss, initialised RAM
// (globram.data) and const ROM data.
#include "loomcc-test.h"
extern i16 tcc_counter;              /* 816-tcc .bss */
extern i16 tcc_initialised;          /* 816-tcc globram.data */
extern const u8 tcc_table[4];        /* 816-tcc .rodata */
i16 unit_value = 1234;               /* ours, read by 816-tcc */
void tcc_bump(void);
i16 tcc_read_unit(void);
int main(void) {
  CHECK(tcc_counter == 0);
  CHECK(tcc_initialised == -77);
  CHECK(tcc_table[3] == 0xfe);
  tcc_bump(); tcc_bump();
  CHECK(tcc_counter == 2);
  tcc_counter = 40;
  tcc_bump();
  CHECK(tcc_counter == 41);
  CHECK(tcc_read_unit() == 1234);
  unit_value = -1;
  CHECK(tcc_read_unit() == -1);
  return 0;
}
