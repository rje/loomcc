// loomcc-do: preprocess
// The GCC manual's example of indirect self-reference.
#define x (4 + y)
#define y (2 * x)
x y
// loomcc-expect: (4 + (2 * x)) (2 * (4 + y))
