// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-no-redef-diag] 816-tcc accepts incompatible macro redefinitions silently
#define X 1
#define X 2 // loomcc-diagnostic: redefin
X
// loomcc-expect: 2
