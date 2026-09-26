// loomcc-do: preprocess
#define f(x) f(x)
f(1) f(f(1))
// loomcc-expect: f(1) f(f(1))
