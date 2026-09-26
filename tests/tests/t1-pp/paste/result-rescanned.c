// loomcc-do: preprocess
#define ab 99
#define cat(a, b) a ## b
cat(a, b) cat(a, b)+1
// loomcc-expect: 99 99+1
