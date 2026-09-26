// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-line-after] 816-tcc reports #if/#ifdef errors on the line after the directive
#if defined // loomcc-error
#endif
