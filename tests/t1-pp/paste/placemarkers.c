// loomcc-do: preprocess
#define cat(a, b) a ## b
[cat(, x)] [cat(x, )] [cat(, )] [cat(,)]
// loomcc-expect: [x] [x] [] []
