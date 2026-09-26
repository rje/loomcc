/* e_pragma.c:  Erroneous use of _Pragma() operator */

/* { dg-do preprocess } */
/* { dg-options "-std=c99 -pedantic-errors" } */

/* Operand of _Pragma() should be a string literal  */
    _Pragma( This is not a string literal)
/* { dg-error "_Pragma takes a parenthesized string literal| Operand of _Pragma\\(\\) is not a string literal" "" { target *-*-* } 7 } */

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_pragma.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-error@7
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: missing error at e_pragma.c:7 (directive line 12)
