// loomcc-do: run
// loomcc-int: agnostic
// A hook record: a function pointer plus a context pointer, as Loom's user
// hook tables pass state.
#include "loomcc-test.h"
typedef struct Hook { void (*fn)(void *ctx, i16 arg); void *ctx; } Hook;
typedef struct { i16 total; u8 calls; } Tally;
static void add(void *ctx, i16 arg) { Tally *t = (Tally *)ctx; t->total = (i16)(t->total + arg); t->calls++; }
static void sub(void *ctx, i16 arg) { Tally *t = (Tally *)ctx; t->total = (i16)(t->total - arg); t->calls++; }
int main(void) {
  Tally a, b;
  Hook hooks[3];
  u8 i;
  a.total = 0; a.calls = 0; b.total = 100; b.calls = 0;
  hooks[0].fn = add; hooks[0].ctx = &a;
  hooks[1].fn = sub; hooks[1].ctx = &b;
  hooks[2].fn = add; hooks[2].ctx = &b;
  for (i = 0; i < 3; i++) hooks[i].fn(hooks[i].ctx, (i16)(i + 1) * 10);
  CHECK(a.total == 10 && a.calls == 1 && b.total == 110 && b.calls == 2);
  return 0;
}
