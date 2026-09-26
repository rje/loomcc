// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-skipped-apostrophe] 816-tcc rejects an unmatched ' in a skipped group
// loomcc-note: In a skipped group an apostrophe (as in English text) must not
// loomcc-note: stop the preprocessor.
#if 0
it's not C
#endif
ok
// loomcc-expect: ok
