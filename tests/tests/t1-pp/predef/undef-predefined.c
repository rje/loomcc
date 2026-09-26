// loomcc-do: preprocess
// loomcc-note: C17 6.10.8p2: predefined macro names shall not be #undef'd
// loomcc-note: (undefined behaviour); gcc and clang warn, and loomcc should too.
// loomcc-ref-diverges: tcc [tcc-define-defined] 816-tcc lets predefined macros be undefined silently
#undef __FILE__ // loomcc-diagnostic
#undef __STDC__ // loomcc-diagnostic
