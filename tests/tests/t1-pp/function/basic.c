// loomcc-do: preprocess
#define sq(x) ((x) * (x))
sq(3) sq(a + b)
// loomcc-expect: ((3) * (3)) ((a + b) * (a + b))
