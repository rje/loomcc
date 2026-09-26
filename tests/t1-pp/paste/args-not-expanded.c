// loomcc-do: preprocess
#define N 3
#define cat(a, b) a ## b
#define xcat(a, b) cat(a, b)
cat(N, 1) xcat(N, 1) cat(N, N) xcat(N, N)
// loomcc-expect: N1 31 NN 33
