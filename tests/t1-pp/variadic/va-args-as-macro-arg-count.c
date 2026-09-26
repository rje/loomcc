// loomcc-do: preprocess
// __VA_ARGS__ with a comma counts as two arguments when passed on.
#define two(a, b) [a][b]
#define pass(...) two(__VA_ARGS__)
pass(1, 2) pass((1, 2), 3)
// loomcc-expect: [1][2] [(1, 2)][3]
