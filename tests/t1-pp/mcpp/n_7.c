/* n_7.c:   #line directive.    */

/* 7.1: Line number and filename.   */
/*  1234; "cpp";    */
#line   1234    "cpp"
    __LINE__; __FILE__;

/* 7.2: Filename argument is optional.  */
/*  2345; "cpp";    */
#line   2345
    __LINE__; __FILE__;

/* 7.3: Argument with macro.    */
/*  3456; "n_7.c" or "prefix/n_7.c";  */
#define LINE_AND_FILENAME   3456 "n_7.c"
#line   LINE_AND_FILENAME
    __LINE__; __FILE__;

/* { dg-do preprocess }
   { dg-final { if ![file exist n_7.i] { return }                       } }
   { dg-final { if \{ [grep n_7.i "1234 *; *\"cpp\" *;"] != ""  \} \{   } }
   { dg-final { if \{ [grep n_7.i "2345 *; *\"cpp\" *;"] != ""  \} \{   } }
   { dg-final { if \{ [grep n_7.i "3456 *; *\"\[^ \]*n_7.c\" *;"] != "" \} \{   } }
   { dg-final { return \} \} \}                                         } }
   { dg-final { fail "n_7.c: #lines"                                    } }
 */

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/n_7.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-note: expected output: clang -E -P -std=c17, checked against mcpp's dg-final patterns
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: tcc timed out
