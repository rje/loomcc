/* e_ucn.c:     Errors of Universal-character-name sequense.    */

/* { dg-do preprocess } */
/* { dg-options "-std=c99 -pedantic-errors" } */

#define macro\U0000001F /* violation of constraint  */
/* { dg-error "universal-character-name| UCN cannot specify the value" "" { target *-*-* } 6 } */
#define macro\uD800     /* violation of constraint (only C, not for C++)    */
/* { dg-error "universal-character-name| UCN cannot specify the value" "" { target *-*-* } 8 } */
#define macro\u123      /* too short sequence (violation of syntax rule)    */
/* { dg-error "incomplete universal-character-name| Illegal UCN sequence" "" { target *-*-* } 10 } */
#define macro\U1234567  /* also too short sequence  */
/* { dg-error "incomplete universal-character-name| Illegal UCN sequence" "" { target *-*-* } 12 } */

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/e_ucn.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-error@6
// loomcc-error@8
// loomcc-error@10
// loomcc-error@12
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: missing error at e_ucn.c:8 (directive line 18)
