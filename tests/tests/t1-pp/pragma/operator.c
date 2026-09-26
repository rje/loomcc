// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-no-pragma-op] 816-tcc -E drops _Pragma
_Pragma("foo bar") x
// loomcc-expect: #pragma foo bar
// loomcc-expect: x
