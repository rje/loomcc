// loomcc-do: preprocess
#define N 2
#define v(...) [__VA_ARGS__]
v(N) v(N, N)
// loomcc-expect: [2] [2, 2]
