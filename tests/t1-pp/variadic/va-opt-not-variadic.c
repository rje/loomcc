// loomcc-do: preprocess
// C23 6.10.4.1: __VA_OPT__ only in a variadic macro's replacement list.
// loomcc-ref-diverges: tcc [tcc-no-va-opt] 816-tcc has no __VA_OPT__
#define F(x) __VA_OPT__(x) // loomcc-diagnostic: __VA_OPT__
