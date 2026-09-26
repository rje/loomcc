// loomcc-do: preprocess
// The arguments come from the source after the object-like macro.
#define A B
#define B(x) [x]
A(1) A (2) A
// loomcc-expect: [1] [2] B
