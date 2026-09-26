// loomcc-do: preprocess
#define g(x) x(1)
#define h(y) [y]
g(h)
// loomcc-expect: [1]
