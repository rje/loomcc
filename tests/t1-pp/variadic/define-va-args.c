// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-va-args-anywhere] 816-tcc accepts __VA_ARGS__ anywhere
#define __VA_ARGS__ 1 // loomcc-diagnostic: __VA_ARGS__
