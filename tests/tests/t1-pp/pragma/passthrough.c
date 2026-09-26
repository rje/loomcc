// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-no-pragma-E] 816-tcc -E drops #pragma lines
#pragma foo bar
x
// loomcc-expect: #pragma foo bar
// loomcc-expect: x
