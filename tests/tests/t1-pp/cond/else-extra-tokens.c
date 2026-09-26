// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-extra-tokens] 816-tcc ignores tokens after #endif/#else/#ifdef
#if 0
#else junk // loomcc-diagnostic
#endif
