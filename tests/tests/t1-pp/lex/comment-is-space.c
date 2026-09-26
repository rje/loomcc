// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-E-glue] 816-tcc -E drops the space between separate tokens
// A comment is one space: it separates tokens, it never joins them.
a/**/b
a/* x */+/* y */+b
// loomcc-expect: a b
// loomcc-expect: a + + b
