// loomcc-do: preprocess
#define LONG(a, b) \
  a /* comment */ + \
  b
LONG(1, 2)
// loomcc-expect: 1 + 2
