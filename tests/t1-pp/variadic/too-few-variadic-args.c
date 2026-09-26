// loomcc-do: preprocess
// C17 6.10.3p4 (constraint): a variadic macro needs more arguments than it
// has named parameters, so M(x) is not valid C17 (C23 allows it).
// loomcc-ref-diverges: tcc [tcc-lax-params] 816-tcc accepts it silently
#define M(X, ...) X
M(x) // loomcc-diagnostic
M(x, y)
// loomcc-expect: x x
