// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-extra-tokens] 816-tcc ignores tokens after #endif/#else/#ifdef
#ifdef X Y // loomcc-diagnostic
#endif
