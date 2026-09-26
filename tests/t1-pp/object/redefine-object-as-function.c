// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-no-redef-diag] 816-tcc accepts incompatible macro redefinitions silently
#define X 1
#define X(a) a // loomcc-diagnostic: redefin
X(2)
// loomcc-expect: 2
