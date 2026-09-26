// loomcc-do: preprocess
// loomcc-note: C17 6.10.3.5p4 EXAMPLE 2
#define max(a, b) ((a) > (b) ? (a) : (b))
max(x, y+1)
// loomcc-expect: ((x) > (y+1) ? (x) : (y+1))
