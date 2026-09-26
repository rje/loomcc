/* e_14_9.c:    Out of range in #if expression (division by 0). */

/* { dg-do preprocess } */

/* 14.9:    Divided by 0.   */
#if     1 / 0       /* { dg-error "(D|d)ivision by zero" } */ // loomcc-error
#endif

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_14_9.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: missing error at e_14_9.c:6 (directive line 6)
