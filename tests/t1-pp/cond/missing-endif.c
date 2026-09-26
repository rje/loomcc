// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-missing-endif-eof] 816-tcc reports a missing #endif at the end of the file
// loomcc-note: clang reports the unterminated #if at the #if line.
#if 1 // loomcc-error: (endif|unterminated)
a
