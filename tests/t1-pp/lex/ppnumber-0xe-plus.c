// loomcc-do: preprocess
// The famous one: 0xe+1 is a single (invalid) pp-number, not 0xe + 1.
#define e 7
0xe+1 0xe +1 1E+e
// loomcc-expect: 0xe+1 0xe +1 1E+e
