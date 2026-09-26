// loomcc-do: preprocess
#define f(x) x
#define g f(g)
g
// loomcc-expect: g
