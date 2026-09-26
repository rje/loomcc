/* n_13_8.t:    Grouping of sub-expressions in #if expression.  */

/* 13.8:    Unary operators are grouped from right to left. */
#if     (- -1 != 1) || (!!9 != 1) || (-!+!9 != -1) || (~~1 != 1)
#error  Bad grouping of -, +, !, ~ in #if expression.
#endif

/* 13.9:    ?: operators are grouped from right to left.    */
#if     (1 ? 2 ? 3 ? 3 : 2 : 1 : 0) != 3
#error  Bad grouping of ? : in #if expression.
#endif

/* 13.10:   Other operators are grouped from left to right. */
#if     (15 >> 2 >> 1 != 1) || (3 << 2 << 1 != 24)
#error  Bad grouping of >>, << in #if expression.
#endif

/* 13.11:   Test of precedence. */
#if     3*10/2 >> !0*2 >> !+!-9 != 1
#error  Bad grouping of -, +, !, *, /, >> in #if expression.
#endif

/* 13.12:   Overall test.  Grouped as:
        ((((((+1 - -1 - ~~1 - -!0) & 6) | ((8 % 9) ^ (-2 * -2))) >> 1) == 7)
        ? 7 : 0) != 7
    evaluated to FALSE.
 */
#if     (((+1- -1-~~1- -!0&6|8%9^-2*-2)>>1)==7?7:0)!=7
#error  Bad arithmetic of #if expression.
#endif

/* { dg-do preprocess } */

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/n_13_8.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-note: expected output: clang -E -P -std=c17, checked against mcpp's dg-final patterns
