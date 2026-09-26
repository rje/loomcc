// loomcc-do: preprocess
// Several splices in a row, including one at the start of a line.
#define LONG 1 + \
  2 + \
\
  3
LONG
// loomcc-expect: 1 + 2 + 3
