/* e_14_7.c:    There is no keyword in #if expression.  */

/* { dg-do preprocess }
   { dg-options "-ansi -w" }
 */

/* 14.7:    sizeof operator is disallowed.  */
/*  Evaluated as: 0 (0)
    Constant expression syntax error.   */
#if     sizeof (int)    /* { dg-error "missing binary operator|(parse|syntax) error| Operator \"\\(\" in incorrect context" } */ // loomcc-error
#endif

/* 14.8:    type cast is disallowed.    */
/*  Evaluated as: (0)0x8000
    Also a constant expression error.   */
#if     (int)0x8000 < 0 /* { dg-error "missing binary operator|(parse|syntax) error| Misplaced constant" } */ // loomcc-error
#endif

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_14_7.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: missing error at e_14_7.c:16 (directive line 16)
