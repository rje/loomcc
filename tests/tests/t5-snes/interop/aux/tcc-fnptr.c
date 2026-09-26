typedef short i16;
typedef unsigned char u8;
typedef i16 (*op_fn)(i16, i16);
static i16 add(i16 a, i16 b) { return a + b; }
static i16 sub(i16 a, i16 b) { return a - b; }
static op_fn registered;
op_fn tcc_get_op(u8 which) { return which ? sub : add; }
void tcc_register(op_fn f) { registered = f; }
i16 tcc_run_registered(i16 a, i16 b) { return registered(a, b); }
