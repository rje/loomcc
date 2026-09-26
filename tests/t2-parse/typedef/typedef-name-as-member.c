// loomcc-do: syntax
// Member names live in their own name space.
typedef int T;
struct S { T T; int U; };
typedef struct S U;
int get(U *u) { return u->T + u->U; }
