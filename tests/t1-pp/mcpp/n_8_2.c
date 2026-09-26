/* n_8_2.c:     Argument of #error is optional. */
/* { dg-do preprocess } */

/* 8.2: #error should be executed.  */
#error      /* { dg-error "#error" }    */ // loomcc-error

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/n_8_2.c (BSD-2-Clause, see LICENSE in this directory)
