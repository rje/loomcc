// loomcc-do: preprocess
#define cat(a, b) a ## b
#define ab cat(a, b)
ab cat(a, b)
// loomcc-expect: ab cat(a, b)
