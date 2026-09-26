// loomcc-do: run
// loomcc-int: agnostic
#include "loomcc-test.h"
typedef void (*visit_fn)(void *ctx, i16 v);
static void each(const i16 *a, int n, visit_fn f, void *ctx) { int i; for (i = 0; i < n; i++) f(ctx, a[i]); }
struct Acc { i32 sum; i16 max; };
static void acc(void *ctx, i16 v) { struct Acc *a = (struct Acc *)ctx; a->sum += v; if (v > a->max) a->max = v; }
int main(void) {
  static const i16 v[] = { 3, 30000, -5, 20000 };
  struct Acc a;
  a.sum = 0; a.max = -32768;
  each(v, 4, acc, &a);
  CHECK(a.sum == 49998 && a.max == 30000);
  return 0;
}
