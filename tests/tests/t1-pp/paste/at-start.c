// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-lax-params] 816-tcc accepts ## at either end of a replacement list
// C17 6.10.3.3p1: ## shall not begin a replacement list.
#define bad ## x // loomcc-error: ##
