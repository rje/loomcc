typedef unsigned char u8;
typedef signed char i8;
typedef unsigned short u16;
u16 unit_u8_sum(u8 a, u8 b, u16 c, u8 d, u8 *p, i8 e);
u16 tcc_u8_sum(u8 a, u8 b, u16 c, u8 d, u8 *p, i8 e) { return a + b + c + d + *p + e; }
u16 tcc_calls_unit(void) { u8 v = 50; return unit_u8_sum(10, 20, 30, 40, &v, -60); }
