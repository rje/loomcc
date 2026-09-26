// loomcc-do: preprocess
// loomcc-note: C23 6.10.5.1 EXAMPLE: # applied to __VA_OPT__.
// loomcc-ref-diverges: tcc [tcc-no-va-opt] 816-tcc has no __VA_OPT__
#define H3(X, ...) #__VA_OPT__(X##X X##X)
H3(, 0)
#define S(...) #__VA_OPT__(a __VA_ARGS__ b)
S() S(1) S(1, 2)
// loomcc-expect: ""
// loomcc-expect: "" "a 1 b" "a 1, 2 b"
