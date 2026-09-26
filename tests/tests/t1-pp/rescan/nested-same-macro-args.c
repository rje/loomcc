// loomcc-do: preprocess
#define add(a, b) (a + b)
add(add(1, 2), add(3, add(4, 5)))
// loomcc-expect: ((1 + 2) + (3 + (4 + 5)))
