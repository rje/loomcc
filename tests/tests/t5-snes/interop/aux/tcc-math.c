/* Compiled by 816-tcc in every mode (loomcc-tcc-sources). */
typedef short i16;
typedef unsigned short u16;
typedef unsigned char u8;
i16 tcc_add(i16 a, i16 b) { return a + b; }
u8 tcc_low(u16 v) { return (u8)v; }
i16 tcc_mix(u8 a, i16 b, u8 c, char *p) { return a + b + c + *p; }
u16 tcc_ptr_diff(u8 *a, u8 *b) { return (u16)(a - b); }
