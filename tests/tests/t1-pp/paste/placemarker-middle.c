// loomcc-do: preprocess
#define f(a, b, c) [a ## b ## c]
f(,,) f(1,,) f(,,3) f(,2,)
// loomcc-expect: [] [1] [3] [2]
