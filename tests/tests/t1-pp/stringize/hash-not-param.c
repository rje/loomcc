// loomcc-do: preprocess
// C17 6.10.3.2p1: in a function-like macro, # must be followed by a parameter.
// loomcc-ref-diverges: tcc [tcc-lax-params] 816-tcc accepts # before a non-parameter
#define f(x) #y // loomcc-error
