/* e_29_3.c:    #undef errors.  */

/* { dg-do preprocess } */

/* 29.3:    Not an identifier.  */
#undef  "string"    /* { dg-error "macro names must be identifiers| invalid macro name| Not an identifier" } */ // loomcc-error
#undef  123         /* { dg-error "macro names must be identifiers| invalid macro name| Not an identifier" } */ // loomcc-error

/* 29.4:    Excessive token sequence.   */
#undef  MACRO_0     Junk    /* { dg-error "extra tokens| garbage after| Excessive token sequence" } */ // loomcc-error

/* 29.5:    No argument.    */
#undef      /* { dg-error "no macro name| invalid macro name| No identifier" } */ // loomcc-error

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_29_3.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: missing error at e_29_3.c:6 (directive line 6)
