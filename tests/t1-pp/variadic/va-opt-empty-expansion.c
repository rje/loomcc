// loomcc-do: preprocess
// __VA_OPT__ looks at whether the variable arguments expand to nothing.
// loomcc-ref-diverges: tcc [tcc-no-va-opt] 816-tcc has no __VA_OPT__
#define EMPTY
#define F(...) <__VA_OPT__(has)>
F() F(EMPTY) F(EMPTY EMPTY) F(0) F(,)
// loomcc-expect: <> <> <> <has> <has>
