/* n_line.c:    line number argument of #line   */
/* C99: Range of line number in #line directive is [1..2147483647]  */

/*  2147483647; */
#line   2147483647
    __LINE__;

/* { dg-do preprocess }
   { dg-options "-std=c99 -w" }
   { dg-final { if ![file exist n_line.i] { return }                    } }
   { dg-final { if \{ [grep n_line.i "2147483647"] != ""        \} \{   } }
   { dg-final { return \}                                               } }
   { dg-final { fail "n_line.c: line number in C99"                     } }
 */

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/n_line.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-note: expected output: clang -E -P -std=c17, checked against mcpp's dg-final patterns
