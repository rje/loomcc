// loomcc-do: preprocess
// C23: __VA_OPT__ shall not appear inside __VA_OPT__.
// loomcc-ref-diverges: tcc [tcc-no-va-opt] 816-tcc has no __VA_OPT__
#define F(...) __VA_OPT__(__VA_OPT__()) // loomcc-error: __VA_OPT__
