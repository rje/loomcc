// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-no-pragma-op] 816-tcc -E drops _Pragma
a _Pragma("mid") b
// loomcc-expect: a
// loomcc-expect: #pragma mid
// loomcc-expect: b
