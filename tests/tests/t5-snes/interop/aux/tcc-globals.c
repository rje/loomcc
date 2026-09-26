typedef short i16;
typedef unsigned char u8;
i16 tcc_counter;
i16 tcc_initialised = -77;
const u8 tcc_table[4] = { 1, 2, 3, 0xfe };
extern i16 unit_value;
void tcc_bump(void) { tcc_counter++; }
i16 tcc_read_unit(void) { return unit_value; }
