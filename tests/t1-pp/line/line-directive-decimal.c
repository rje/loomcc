// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-line-octal] 816-tcc reads the #line number as octal
// The #line digit sequence is decimal even with a leading zero.
#line 010
__LINE__
// loomcc-expect: 10
