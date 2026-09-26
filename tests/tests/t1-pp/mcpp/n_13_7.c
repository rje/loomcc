/* n_13_7.c:    Short-circuit evaluation of #if expression. */

/* 13.7:    10/0 or 10/MACRO_0 are never evaluated, "divide by zero" error
        cannot occur.   */
#define MACRO_0     0

/*  Valid block */
#if     0 && 10 / 0
    Block to be skipped
#endif
#if     not_defined && 10 / not_defined
    Block to be skipped
#endif
#if     MACRO_0 && 10 / MACRO_0 > 1
    Block to be skipped
#endif
#if     MACRO_0 ? 10 / MACRO_0 : 0
    Block to be skipped
#endif
#if     MACRO_0 == 0 || 10 / MACRO_0 > 1
    Valid block
#else
    Block to be skipped
#endif

/* { dg-do preprocess }
   { dg-options "-ansi -w" }
   { dg-final { if ![file exist n_13_7.i] { return }                    } }
   { dg-final { if \{ [grep n_13_7.i "Valid block"] != ""       \} \{   } }
   { dg-final { return \}                                               } }
   { dg-final { fail "n_13_7.c: short-circuit evaluation of #if expression" } }
 */

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/n_13_7.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-note: expected output: clang -E -P -std=c17, checked against mcpp's dg-final patterns
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: unexpected error at n_13_7.c:9: division by zero in constant
