/* e_intmax.c:  Overflow of constant expression in #if directive.    */

/* { dg-do preprocess } */
/* { dg-options "-std=c99 -pedantic-errors -w" } */

#define	INTMAX_MAX	0x7FFFFFFFFFFFFFFF
#define INTMAX_MIN	(-INTMAX_MAX-1)
#define SHRT_MAX    0x7FFF

#if     INTMAX_MAX - INTMAX_MIN /* { dg-error "integer overflow in preprocessor expression| Result of \"-\" is out of range" } */ // loomcc-error
#endif
#if     INTMAX_MAX + 1 > SHRT_MAX   /* { dg-error "integer overflow in preprocessor expression| Result of \"\\+\" is out of range" } */ // loomcc-error
#endif
#if     INTMAX_MIN - 1  /* { dg-error "integer overflow in preprocessor expression| Result of \"-\" is out of range" } */ // loomcc-error
#endif
#if     INTMAX_MAX * 2  /* { dg-error "integer overflow in preprocessor expression| Result of \"\\*\" is out of range" } */ // loomcc-error
#endif

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_intmax.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: missing error at e_intmax.c:10 (directive line 10)
