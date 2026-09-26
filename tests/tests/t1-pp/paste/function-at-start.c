// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-lax-params] 816-tcc accepts ## at either end of a replacement list
#define bad(x) ## x // loomcc-error: ##
