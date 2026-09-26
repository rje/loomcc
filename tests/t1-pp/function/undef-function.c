// loomcc-do: preprocess
#define f(x) [x]
#undef f
f(1)
// loomcc-expect: f(1)
