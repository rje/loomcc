/* e_27_7.c:    Error of rescanning.    */

/* { dg-do preprocess } */

#define sub( x, y)      (x - y)

/* 27.7:    */
#define TWO_TOKENS      a,b
#define SUB( x, y)      sub( x, y)
/* Too many arguments error while rescanning after once replaced to:
    sub( a,b, 1);   */
    SUB( TWO_TOKENS, 1);
/* { dg-error "passed 3 arguments, but takes just 2| used with too many \\(3\\) args| More than necessary 2 argument\\(s\\) in macro call" "too many arguments in rescanning" { target *-*-* } 12 } */

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_27_7.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-error@12
