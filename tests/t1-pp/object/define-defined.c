// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-define-defined] 816-tcc lets `defined` be defined or undefined
// C17 6.10.8p2 forbids it (undefined behaviour, not a constraint); clang rejects it.
#define defined 1 // loomcc-diagnostic: defined
