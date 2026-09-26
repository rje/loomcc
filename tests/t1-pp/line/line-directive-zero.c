// loomcc-do: preprocess
// loomcc-note: C17 6.10.4p3: the number shall not be 0 (undefined behaviour);
// loomcc-note: clang warns. loomcc should diagnose it.
// loomcc-ref-diverges: tcc [tcc-line-lax] 816-tcc accepts any #line number
#line 0 // loomcc-diagnostic
