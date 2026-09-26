// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-E-glue] 816-tcc -E drops the space between separate tokens
#define f(x) =x=
f(=) f(<) f()
// loomcc-expect: = = = = < = = =
