/* e_17.c:  Ill-formed group in a source file.  */

/* { dg-do preprocess } */

#define MACRO_1     1

/* 17.1:    Error of #endif without #if.    */
#endif  /* { dg-error "#endif without #if| unbalanced `#endif'| Not in a #if \\(#ifdef\\) section" } */ // loomcc-error

/* 17.2:    Error of #else without #if. */
#else   /* { dg-error "#else without #if| `#else' not within a conditional| Not in a #if \\(#ifdef\\) section" } */ // loomcc-error

/* 17.3:    Error of #else after #else. */
#if     MACRO_1
#else                   /* line 15  */
#if     1
#else
#endif
#else   /* { dg-error "#else after #else\n\[\^ \]*( error:|) the conditional began here| `#else' after `#else'\n (matches line 14)| Already seen #else at line 15" } */ // loomcc-error
#endif

/* 17.4:    Error of #elif after #else. */
#if     MACRO_1 == 1
#else                   /* line 24  */
#elif   MACRO_1 == 0    /* { dg-error "#elif after #else\n\[\^ \]*( error:|) the conditional began here| `#elif' after `#else'\n (matches line 23)| Already seen #else at line 24" } */ // loomcc-error
#endif

/* 17.7:    Error of unterminated #if section.  */
#if     MACRO_1 == 0    /* line 29  */
#else
/* { dg-error "unterminated #else| unterminated `#else'| End of input within #if \\(#ifdef\\) section" "" { target *-*-* } 0 } */

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_17.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-error@*
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: missing error at e_17.c:11 (directive line 11)
