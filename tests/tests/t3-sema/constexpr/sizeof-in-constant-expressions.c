// loomcc-do: syntax
#include "loomcc-test.h"
struct Rec { u8 k; i16 v; };
enum { REC_SIZE = sizeof(struct Rec), N = 64 / sizeof(struct Rec) };
static struct Rec pool[N];
STATIC_CHECK(REC_SIZE == 4 && N == 16 && sizeof(pool) == 64);
