// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-fold-host-int] 816-tcc folds (u16)-1 without truncating to 16 bits
#include "loomcc-test.h"
STATIC_CHECK((u8)0x1ff == 0xff);
STATIC_CHECK((i8)0x80 == -128);
STATIC_CHECK((u16)-1 == 0xffffu);
STATIC_CHECK((i16)0x8000u == -32767 - 1);
STATIC_CHECK((u8)(u16)-2 == 254);
