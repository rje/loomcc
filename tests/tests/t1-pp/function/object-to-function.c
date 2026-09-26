// loomcc-do: preprocess
#define f g
#define g(x) x+1
f(2)
// loomcc-expect: 2+1
