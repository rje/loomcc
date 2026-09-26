/* e_14.c:  Illegal #if expressions.    */

/* { dg-do preprocess } */
/* { dg-options "-ansi -pedantic-errors -w" } */

#define A   1
#define B   1

/* 14.1:    String literal is not allowed in #if expression.    */
#if     "string"    /* { dg-error "not (valid|allowed) in (#if|preprocessor)( expressions|)| Can't use a string literal" } */ // loomcc-error
#endif      /* The second error ?   */

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_14.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: missing error at e_14.c:10 (directive line 10)
