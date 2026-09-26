// loomcc-do: preprocess
#define N 5
#define f(x) [x]
f(N) f(f(N))
// loomcc-expect: [5] [[5]]
