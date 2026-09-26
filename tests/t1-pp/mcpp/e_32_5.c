/* e_32_5.c:    Range error of character constant.  */

/* { dg-do preprocess } */

/* 32.5:    Value of a numerical escape sequence in character constant should
        be in the range of char.    */
/* Out of range */
#if     '\x123' == 0x123    /* { dg-error "escape sequence out of range| hex character constant does not fit in a byte| 8 bits can't represent escape sequence" } */ // loomcc-error
#endif

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_32_5.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: missing error at e_32_5.c:8 (directive line 8)
