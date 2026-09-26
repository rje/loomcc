// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-lax-params] 816-tcc accepts a trailing #
#define f(x) x # // loomcc-error
