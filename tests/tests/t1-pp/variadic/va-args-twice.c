// loomcc-do: preprocess
#define v(...) __VA_ARGS__ | __VA_ARGS__
v(a, b)
// loomcc-expect: a, b | a, b
