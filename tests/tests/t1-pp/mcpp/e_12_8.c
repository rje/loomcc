/* e_12_8.c:    Out of range of integer pp-token in #if expression. */

/* { dg-do preprocess } */

/* 12.8:    Preprocessing number perhaps out of range of unsigned long. */
#if     123456789012345678901   /* { dg-error "(C|c)onstant (\"\[0-9\]*\" is |)(out of range|is too large for its type)" } */ // loomcc-error
#endif

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_12_8.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: missing error at e_12_8.c:6 (directive line 6)
