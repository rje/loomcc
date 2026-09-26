/* e_7_4.c:     #line error.    */

/* { dg-do preprocess } */

/* 7.4:     string literal in #line directive shall be a character string
        literal.    */

#line   123     L"wide"     /* { dg-error "not a valid filename| invalid format| Not a file name" } */ // loomcc-error
/*  10; "e_7_4.c";   */
    __LINE__; __FILE__;

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_7_4.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: missing error at e_7_4.c:8 (directive line 8)
