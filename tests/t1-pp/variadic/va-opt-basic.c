// loomcc-do: preprocess
// loomcc-note: __VA_OPT__ is C23 (6.10.4.1); loomcc supports it (PLAN 2.1).
// loomcc-ref-diverges: tcc [tcc-no-va-opt] 816-tcc has no __VA_OPT__
#define F(...) f(0 __VA_OPT__(,) __VA_ARGS__)
F(a, b, c) F() F(x)
// loomcc-expect: f(0, a, b, c) f(0) f(0, x)
