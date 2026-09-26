// loomcc-do: preprocess
#define v(a, ...) a: __VA_ARGS__
v(x, y) v(x, y, z) v(x,)
// loomcc-expect: x: y x: y, z x:
