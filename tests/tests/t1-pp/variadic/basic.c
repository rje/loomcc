// loomcc-do: preprocess
#define v(...) [__VA_ARGS__]
v(1) v(1, 2) v(a, (b, c), d)
// loomcc-expect: [1] [1, 2] [a, (b, c), d]
