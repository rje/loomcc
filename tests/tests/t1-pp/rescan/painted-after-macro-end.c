// loomcc-do: preprocess
// g expands to f inside f's context-derived hide set: f(2) is not invoked.
#define f(x) x g
#define g f
f(1)(2)
// loomcc-expect: 1 f(2)
