// loomcc-do: preprocess
// A directive name is never macro-expanded.
#define define undef
#define X 1
X
// loomcc-expect: 1
