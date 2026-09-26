/* n_29.c:  #undef directive.   */

/* 29.1:    Undefined macro is not a macro. */
/*  DEFINED;    */
#define DEFINED
#undef  DEFINED
    DEFINED;

/* 29.2:    Undefining undefined name is not an error.  */
#undef  UNDEFINED

/* { dg-do preprocess }
   { dg-final { if ![file exist n_29.i] { return }                      } }
   { dg-final { if \{ [grep n_29.i "DEFINED"] != ""             \} \{   } }
   { dg-final { return \}                                               } }
   { dg-final { fail "n_29.c: #undef"                                   } }
 */

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/n_29.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-note: expected output: clang -E -P -std=c17, checked against mcpp's dg-final patterns
