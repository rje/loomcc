// loomcc-do: preprocess
#define EMPTY()
#define DEFER(id) id EMPTY()
#define EXPAND(x) x
#define A() 123
EXPAND(DEFER(A)())
// loomcc-expect: 123
