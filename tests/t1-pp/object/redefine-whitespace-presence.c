// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-no-redef-diag] 816-tcc accepts incompatible macro redefinitions silently
// C17 6.10.3p2: whitespace separation counts; `1+2` differs from `1 + 2`.
#define X 1+2
#define X 1 + 2 // loomcc-diagnostic: redefin
X
// loomcc-expect: 1 + 2
