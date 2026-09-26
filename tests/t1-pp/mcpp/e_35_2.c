/* e_35_2.c:    Out of range of character constant. */

/* { dg-do preprocess } */

/* 35.2:    */
/* Perhaps out of range.    */
#if     'abcdefghi' /* { dg-error "character constant too long| out of range" } */ // loomcc-error
#endif

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_35_2.c (BSD-2-Clause, see LICENSE in this directory)
