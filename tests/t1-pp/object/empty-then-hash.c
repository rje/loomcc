// loomcc-do: preprocess
// The line starts with a macro, so the # is not a directive introducer.
// loomcc-ref-diverges: tcc [tcc-hash-midline] 816-tcc treats the # as a directive
#define EMPTY
EMPTY # define Y 2
Y
// loomcc-expect: # define Y 2 Y
