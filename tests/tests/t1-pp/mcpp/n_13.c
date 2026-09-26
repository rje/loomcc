/* n_13.c:  Valid operators in #if expression.  */

/* Valid operators are (precedence in this order) :
    defined, (unary)+, (unary)-, ~, !,
    *, /, %,
    +, -,
    <<, >>,
    <, >, <=, >=,
    ==, !=,
    &,
    ^,
    |,
    &&,
    ||,
    ? :
 */

/* 13.1:    Bit shift.  */
#if     1 << 2 != 4 || 8 >> 1 != 4
#error  Bad arithmetic of <<, >> operators.
#endif

/* 13.2:    Bitwise operators.  */
#if     (3 ^ 5) != 6 || (3 | 5) != 7 || (3 & 5) != 1
#error  Bad arithmetic of ^, |, & operators.
#endif

/* 13.3:    Result of ||, && operators is either of 1 or 0. */
#if     (2 || 3) != 1 || (2 && 3) != 1 || (0 || 4) != 1 || (0 && 5) != 0
#error  Bad arithmetic of ||, && operators.
#endif

/* 13.4:    ?, : operator.  */
#if     (0 ? 1 : 2) != 2
#error  Bad arithmetic of ?: operator.
#endif

/* { dg-do preprocess } */

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/n_13.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-note: expected output: clang -E -P -std=c17, checked against mcpp's dg-final patterns
