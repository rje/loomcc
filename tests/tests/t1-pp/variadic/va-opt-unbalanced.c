// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-no-va-opt] 816-tcc has no __VA_OPT__
#define F(...) __VA_OPT__(a // loomcc-error
