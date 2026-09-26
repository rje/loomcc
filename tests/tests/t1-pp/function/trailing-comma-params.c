// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-lax-params] 816-tcc accepts malformed macro parameter lists
#define f(a,) a // loomcc-error
