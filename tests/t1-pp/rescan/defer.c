// loomcc-do: preprocess
#define EMPTY()
#define DEFER(id) id EMPTY()
#define A() 123
DEFER(A)()
// loomcc-expect: A ()
