/* n_22.c:  Tokenization of preprocessing number.   */

#define EXP         1

/* 22.1:    12E+EXP is a preprocessing number, EXP is not expanded. */
/*  12E+EXP;    */
    12E+EXP;

/* 22.2:    .2e-EXP is also a preprocessing number. */
/*  .2e-EXP;    */
    .2e-EXP;

/* 22.3:    + or - is allowed only following E or e, 12+EXP is not a
        preprocessing number.   */
/* Three tokens: 12 + 1;    */
    12+EXP;

/* { dg-do preprocess }
   { dg-final { if ![file exist n_22.i] { return }                      } }
   { dg-final { if \{ [grep n_22.i "12E\\+EXP"] != ""           \} \{   } }
   { dg-final { if \{ [grep n_22.i "\.2e-EXP"] != ""            \} \{   } }
   { dg-final { if \{ [grep n_22.i "12\[ \]*\\+\[ \]*1"] != ""  \} \{   } }
   { dg-final { return \} \} \}                                         } }
   { dg-final { fail "n_22.c: tokenization of pp-number"                } }
 */

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/n_22.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-note: expected output: clang -E -P -std=c17, checked against mcpp's dg-final patterns
