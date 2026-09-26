/* support file for rom-tables-many-banks.c: one 20000-byte table per unit,
 * so each gets its own .rodata section (and bank). */
const unsigned char t1[20000] = { 20, [19999] = 21 };
