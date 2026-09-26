/* e_16.c:  Trailing junk of #else, #endif. */

/* { dg-do preprocess } */

/* 16.1:    Trailing junk of #else. */
#define MACRO_0     0
#if     MACRO_0
#else   MACRO_0     /* { dg-error "extra tokens|text following| Excessive token sequence" } */ // loomcc-error

/* 16.2:    Trailing junk of #endif.    */
#endif  MACRO_0     /* { dg-error "extra tokens|text following| Excessive token sequence" } */ // loomcc-error

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_16.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: missing error at e_16.c:8 (directive line 8)
