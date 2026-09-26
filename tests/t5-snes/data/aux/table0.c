/* support file for rom-tables-many-banks.c: one 20000-byte table per unit,
 * so each gets its own .rodata section (and bank). */
const unsigned char t0[20000] = { 10, [19999] = 11 };
