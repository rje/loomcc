// loomcc-do: preprocess
// loomcc-note: The deferred-recursion idiom (Boost.PP, Cloak, P99). After A's
// loomcc-note: expansion is over, the A that B's expansion produces is not
// loomcc-note: painted: C17 6.10.3.4p2 paints only names met while that macro
// loomcc-note: is being replaced. gcc, clang and MSVC agree. A pure Prosser
// loomcc-note: hide-set implementation paints it (A is in the hide set of the
// loomcc-note: `()` that B's invocation takes from A's replacement) and
// loomcc-note: prints `1 2 A ()` for the last line.
#define EMPTY()
#define DEFER(id) id EMPTY()
#define EXPAND(...) __VA_ARGS__
#define A() 1 DEFER(B)()
#define B() 2 DEFER(A)()
A()
EXPAND(A())
EXPAND(EXPAND(A()))
// loomcc-expect: 1 B ()
// loomcc-expect: 1 2 A ()
// loomcc-expect: 1 2 1 B ()
