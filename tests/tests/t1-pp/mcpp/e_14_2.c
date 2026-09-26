/* e_14_2.c:    Illegal #if expressions-2.  */

/* { dg-do preprocess } */
/* { dg-options "-ansi -pedantic-errors -w" } */

#define A   1
#define B   1

/* 14.2:    Operators =, +=, ++, etc. are not allowed in #if expression.*/

#if     A = B   /* { dg-error "is not valid| (parse|syntax) error| Can't use the operator" } */ // loomcc-error
#endif
#if     A++ B   /* { dg-error "is not (valid|allowed)| Can't use the operator" } */ // loomcc-error
#endif
#if     A.B     /* { dg-error "is not valid| (parse|syntax) error| empty #if expression| Can't use the operator" } */ // loomcc-error
#endif

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_14_2.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: missing error at e_14_2.c:11 (directive line 11)
