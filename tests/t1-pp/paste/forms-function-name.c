// loomcc-do: preprocess
#define fg(x) [x]
#define cat(a, b) a ## b
cat(f, g)(1) cat(f, g) (2) cat(f, g)
// loomcc-expect: [1] [2] fg
