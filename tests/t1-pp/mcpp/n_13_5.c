/* n_13_5.c:    Arithmetic conversion in #if expressions.   */

/* 13.5:    The usual arithmetic conversion is not performed on bit shift.  */
#if     -1 << 3U > 0
#error  Bad conversion of bit shift operands.
#endif

/* 13.6:    Usual arithmetic conversions.   */
#if     -1 <= 0U        /* -1 is converted to unsigned long.    */
#error  Bad arithmetic conversion.
#endif

#if     -1 * 1U <= 0
#error  Bad arithmetic conversion.
#endif

/* Second and third operands of conditional operator are converted to the
#error      same type, thus -1 is converted to unsigned long.    */
#if     (1 ? -1 : 0U) <= 0
#error  Bad arithmetic conversion.
#endif

/* { dg-do preprocess }
   { dg-options "-ansi -w" }
 */

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/n_13_5.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-note: expected output: clang -E -P -std=c17, checked against mcpp's dg-final patterns
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: unexpected error at n_13_5.c:20: #error Bad arithmetic conversion.
