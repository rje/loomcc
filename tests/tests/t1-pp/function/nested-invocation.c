// loomcc-do: preprocess
#define f(x) (x)
f(f(1)) f(f(f(2)))
// loomcc-expect: ((1)) (((2)))
