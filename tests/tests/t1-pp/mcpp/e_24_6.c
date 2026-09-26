/* e_24_6.c:    Operand of # operator in function-like macro definition shall
        be a parameter name.    */

/* { dg-do preprocess } */

/* 24.6:    */
#define FUNC( a)    # b     /* { dg-error "not followed by a macro parameter| should be followed by a macro argument name| Not a formal parameter" } */ // loomcc-error

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_24_6.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: missing error at e_24_6.c:7 (directive line 7)
