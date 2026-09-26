/* e_4_3.c:     Illegal pp-token.   */

/* { dg-do preprocess } */

/* 4.3:     Empty character constant is an error.   */
#if     '' == 0     /* { dg-error "empty character constant| Empty character constant '', skipped the line\n\[^ \]* error:" "" } */ // loomcc-error
#endif  /* Maybe the second error   */

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_4_3.c (BSD-2-Clause, see LICENSE in this directory)
