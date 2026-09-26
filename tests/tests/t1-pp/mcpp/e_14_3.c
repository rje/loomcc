/* e_14_3.c:    Illegal #if expressions-3.  */

/* { dg-do preprocess } */
/* { dg-options "-ansi -pedantic-errors -w" } */

#define A   1
#define B   1

/* 14.3:    Unterminated #if expression.    */
#if     0 <     /* { dg-error "no right operand| (parse|syntax) error| Unterminated expression" } */ // loomcc-error
#endif
#if     ( (A == B)  /* { dg-error "missing '\\)' in expression| (parse|syntax) error| Missing \"\\)\"" } */ // loomcc-error
#endif

/* 14.4:    Unbalanced parenthesis in #if defined operator. */
#if     defined ( MACRO     /* { dg-error "missing '\\)' after \"defined\"| Bad defined syntax" } */ // loomcc-error
#endif

/* 14.5:    No argument.    */
#if             /* { dg-error "#if with no expression| (parse|syntax) error| No argument" } */ // loomcc-error
#endif

/* 14.6:    Macro expanding to 0 token in #if expression.   */
#define ZERO_TOKEN
#if     ZERO_TOKEN  /* { dg-error "#if with no expression| (parse|syntax) error| Unterminated expression" } */ // loomcc-error
#endif

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_14_3.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: missing error at e_14_3.c:10 (directive line 10)
