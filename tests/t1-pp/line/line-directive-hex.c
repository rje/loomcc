// loomcc-do: preprocess
// loomcc-note: The number must be a digit-sequence: 0x10 is not one.
// loomcc-ref-diverges: tcc [tcc-line-lax] 816-tcc accepts any #line number
#line 0x10 // loomcc-error
