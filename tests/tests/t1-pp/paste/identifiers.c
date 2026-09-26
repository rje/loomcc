// loomcc-do: preprocess
#define cat(a, b) a ## b
cat(foo, bar) cat(x, 1) cat(_, 1) cat(a_, _b)
// loomcc-expect: foobar x1 _1 a__b
