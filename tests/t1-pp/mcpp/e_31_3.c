/* e_31_3.c:    Macro call in control line should complete in the line. */

/* { dg-do preprocess } */

#define glue( a, b)     a ## b
#define str( s)         # s
#define xstr( s)        str( s)

/* 31.3:    Unterminated macro call.    */
#include    xstr( glue( header,
    .h))
/* [ dg-error "unterminated argument list| (U|u)nterminated macro call" "" { target *-*-* } 10 } */
/* { dg-excess-errors "" } */

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_31_3.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-error@10
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: missing error at e_31_3.c:10 (directive line 17)
