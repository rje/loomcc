// loomcc-do: syntax
#include "loomcc-test.h"
typedef u8 byte;
typedef byte bytes4[4];
typedef bytes4 *pbytes4;
typedef const pbytes4 cpbytes4;
bytes4 b;
STATIC_CHECK(sizeof(b) == 4 && sizeof(*(pbytes4)0) == 4);
