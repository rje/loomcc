// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-E-glue] 816-tcc -E drops the space between separate tokens
/* // not a line comment */ b
/**/c/***/d/*/ still comment */e
// loomcc-expect: b c d e
