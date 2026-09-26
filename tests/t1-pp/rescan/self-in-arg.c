// loomcc-do: preprocess
#define f(a) a + f(a)
f(x) f(f(x))
// loomcc-expect: x + f(x) x + f(x) + f(x + f(x))
