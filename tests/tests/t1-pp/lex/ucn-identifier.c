// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-no-ucn] 816-tcc has no universal character names
#define Á 1
Á + Á
// loomcc-expect: 1 + 1
