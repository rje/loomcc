// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-no-pragma-E] 816-tcc -E drops #pragma lines
// loomcc-note: Unknown pragmas pass through without macro replacement.
#define X 1
#pragma foo X
// loomcc-expect: #pragma foo X
