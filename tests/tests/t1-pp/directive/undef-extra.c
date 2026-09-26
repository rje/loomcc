// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-extra-tokens] 816-tcc ignores extra tokens
#define X
#undef X Y // loomcc-diagnostic
