// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-stringize-space] 816-tcc drops or misplaces spaces when stringizing
// Whitespace comes from the argument's spelling, not from expansions.
#define s(x) #x
#define xs(x) s(x)
#define E
#define P +
xs(a E b) xs(E a) xs(a P b) xs(aP)
// loomcc-expect: "a b" "a" "a + b" "aP"
