typedef short i16;
typedef unsigned char u8;
i16 unit_twice(i16 x);
i16 tcc_call_direct(i16 x) { return unit_twice(x) + 1; }
i16 tcc_call_pointer(i16 (*f)(i16, u8), i16 x) { return f(x, 3); }
