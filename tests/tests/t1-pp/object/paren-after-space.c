// loomcc-do: preprocess
// With a space before (, the macro is object-like.
#define X (a)
X X(1)
// loomcc-expect: (a) (a)(1)
