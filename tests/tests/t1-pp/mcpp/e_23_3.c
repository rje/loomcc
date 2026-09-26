/* e_23_3.c:    ## operator shall not occur at the beginning or at the end of
        replacement list for either form of macro definition.   */

/* { dg-do preprocess } */

/* 23.3:    In object-like macro.   */
#define con     ## name     /* { dg-error "'##' cannot appear at either end of a macro expansion| `##' at start of macro definition| No token before ##" } */ // loomcc-error
#define cat     12 ##       /* { dg-error "'##' cannot appear at either end of a macro expansion| No token after ##" } */ // loomcc-error

/* 23.4:    In function-like macro. */
#define CON( a, b)  ## a ## b   /* { dg-error "'##' cannot appear at either end of a macro expansion| `##' at start of macro definition| No token before ##" } */ // loomcc-error
#define CAT( b, c)  b ## c ##   /* { dg-error "'##' cannot appear at either end of a macro expansion| No token after ##" } */ // loomcc-error

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_23_3.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: missing error at e_23_3.c:7 (directive line 7)
