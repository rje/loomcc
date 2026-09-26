// loomcc-do: preprocess
// loomcc-note: _Pragma produced inside a macro argument still becomes a pragma.
// loomcc-ref-diverges: tcc [tcc-no-pragma-op] 816-tcc -E drops _Pragma
#define f(x) x
f(_Pragma("inner") y)
// loomcc-expect: #pragma inner
// loomcc-expect: y
