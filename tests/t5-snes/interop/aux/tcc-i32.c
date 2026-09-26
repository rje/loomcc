/* 32-bit integers: long long for 816-tcc, long for loomcc and msp430 */
#ifdef __TINYC__
typedef long long i32;
typedef unsigned long long u32;
#else
typedef long i32;
typedef unsigned long u32;
#endif
typedef unsigned char u8;
typedef short i16;
i32 unit_i32_scale(i32 v, i16 k);
i32 tcc_i32_add(i32 a, i32 b) { return a + b; }
u32 tcc_u32_mix(u8 a, u32 b, i16 c) { return b + a + c; }
i32 tcc_calls_scale(i32 v) { return unit_i32_scale(v, 3); }
