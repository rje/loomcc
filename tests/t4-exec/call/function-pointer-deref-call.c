// loomcc-do: run
// loomcc-int: agnostic
// Calling through an explicitly dereferenced function pointer: (*f)(args).
// loomcc-ref-diverges: tcc-rom [tcc-deref-call-spill] 816-tcc spills f into its stack slot after pushing the arguments (S-relative offset not adjusted), so the call jumps to garbage
#include "loomcc-test.h"
static i16 sub(i16 a, i16 b) { return (i16)(a - b); }
int main(void) {
  i16 (*f)(i16, i16) = sub;
  CHECK((*f)(10, 3) == 7);
  CHECK((**f)(1, 2) == -1);
  return 0;
}
