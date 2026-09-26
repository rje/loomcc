// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-E-glue] 816-tcc -E drops the space between separate tokens
// -E output must not glue tokens that were separate: + + is not ++.
#define p +
#define m -
+p -m p+ m-
// loomcc-expect: + + - - + + - -
