// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-no-pragma-E] 816-tcc -E drops #pragma lines
// loomcc-no-warnings
#pragma STDC FP_CONTRACT ON
#pragma STDC FENV_ACCESS OFF
// loomcc-expect: #pragma STDC FP_CONTRACT ON
// loomcc-expect: #pragma STDC FENV_ACCESS OFF
