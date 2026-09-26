/* e_15_3.c:    #ifdef, #ifndef syntax errors.  */

/* { dg-do preprocess } */

/* 15.3:    Not an identifier.  */
#ifdef  "string"    /* { dg-error "macro names must be identifiers|argument starts with punctuation| Not an identifier" } */ // loomcc-error
#endif
#ifdef  123         /* { dg-error "macro names must be identifiers|argument starts with a digit| Not an identifier" } */ // loomcc-error
#endif

/* 15.4:    Excessive token sequence.   */
#ifdef  MACRO   Junk    /* { dg-error "extra tokens|garbage at end| Excessive token sequence" } */ // loomcc-error
#endif

/* 15.5:    No argument.    */
#ifndef             /* { dg-error "no macro name given| (N|n)o argument" } */ // loomcc-error
#endif

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_15_3.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: missing error at e_15_3.c:8 (directive line 8)
