// loomcc-do: preprocess
#define inner(a, b, c) <a|b|c>
#define outer(...) inner(__VA_ARGS__)
outer(1, 2, 3)
// loomcc-expect: <1|2|3>
