// loomcc-do: preprocess
#define f(x) [x]
f((a, b)) f(((c)))
// loomcc-expect: [(a, b)] [((c))]
