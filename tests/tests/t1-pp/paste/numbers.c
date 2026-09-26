// loomcc-do: preprocess
#define cat(a, b) a ## b
cat(1, 2) cat(1, e) cat(0x, ff) cat(1., 5) cat(1e, +)
// loomcc-expect: 12 1e 0xff 1.5 1e+
