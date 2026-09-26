// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-lax-params] 816-tcc accepts ... before other parameters
#define f(..., a) a // loomcc-error
