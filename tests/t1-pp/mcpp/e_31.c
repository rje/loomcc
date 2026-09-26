/* e_31.c:  Illegal macro calls.    */

/* { dg-do preprocess } */

#define sub( a, b)      (a - b)

/* 31.1:    Too many arguments error.   */
    sub( x, y, z);  /* { dg-error "passed 3 arguments, but takes just 2| used with too many \\(3\\) args| More than necessary 2 argument\\(s\\) in macro call" } */ // loomcc-error

/* 31.2:    Too few arguments error.    */
    sub( x);    /* { dg-error "requires 2 arguments, but only 1 given| used with just one arg| Less than necessary 2 argument\\(s\\) in macro call" } */ // loomcc-error

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_31.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: missing error at e_31.c:11 (directive line 11)
