// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-stringize-space] 816-tcc drops or misplaces spaces when stringizing
#define s(x) #x
s(a/**/b) s( /*c*/ a /*c*/ ) s(a/*c*/+/*c*/b)
// loomcc-expect: "a b" "a" "a + b"
