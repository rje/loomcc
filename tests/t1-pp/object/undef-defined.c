// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-define-defined] 816-tcc lets `defined` be defined or undefined
#undef defined // loomcc-diagnostic: defined
