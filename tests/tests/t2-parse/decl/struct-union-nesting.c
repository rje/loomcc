// loomcc-do: syntax
#include "loomcc-test.h"
struct Outer {
  union { struct { u8 lo, hi; } b; u16 w; } u;
  struct Inner { i16 x, y; } pos[2];
  struct Inner *self;
};
struct Inner standalone;
STATIC_CHECK(sizeof(standalone) == 4);
