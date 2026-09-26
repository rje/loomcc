// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-rvalue-member] 816-tcc rejects a member of a struct rvalue (lvalue expected)
// A member of a struct rvalue (a conditional, a call result, an assignment)
// is a valid expression; it is just not an lvalue (6.5.2.3p3).
#include "loomcc-test.h"
struct s { char c; i16 v; } a, b;
int c;
struct s get(void);
STATIC_CHECK(sizeof((c ? a : b).c) == 1);
STATIC_CHECK(sizeof((a = b).v) == 2);
i16 f(void) { return (c ? a : b).v + get().v + (a = b).c; }
