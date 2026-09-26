// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-line-after] 816-tcc reports #if/#ifdef errors on the line after the directive
#if (1 + 2 // loomcc-error
#endif
