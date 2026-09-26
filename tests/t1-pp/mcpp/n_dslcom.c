/* n_dslcom.c:  // is a comment of C++ and C99. */
/*  a;  */ 
    a;  // is a comment of C++ and C99

/* { dg-do preprocess }
   { dg-options "-std=c99" }
   { dg-final { if ![file exist n_dslcom.i] { return }                  } }
   { dg-final { if \{ [grep n_dslcom.i "^\[ \]*a;\[ \]*$"] != ""    \} \{   } }
   { dg-final { return \}                                               } }
   { dg-final { fail "n_dslcom.c: double slash comment"                 } }
 */

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/n_dslcom.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-note: expected output: clang -E -P -std=c17, checked against mcpp's dg-final patterns
