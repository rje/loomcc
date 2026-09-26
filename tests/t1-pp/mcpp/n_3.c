/* n_3.c:   Handling of comment.    */

/* 3.1: A comment is converted to one space.    */
/*  abc de  */
    abc/* comment */de

/* 3.2: // is not a comment of C.   */
#if 0   /* This feature is obsolete now.  */
/*  // is not a comment of C    */
    // is not a comment of C
#endif

/* 3.3: Comment is parsed prior to the parsing of preprocessing directive.  */
/*  abcd    */
#if     0
    "nonsence"; /*
#else
    still in
    comment     */
#else
#define MACRO_abcd  /*
    in comment
    */  abcd
#endif
    MACRO_abcd

/* { dg-do preprocess }
   { dg-options "-ansi -w" }
   { dg-final { if ![file exist n_3.i] { return }                       } }
   { dg-final { if \{ [grep n_3.i "abc de"] != ""               \} \{   } }
   { dg-final { if \{ [grep n_3.i "abcd"] != ""                 \} \{   } }
   { dg-final { return \} \}                                            } }
   { dg-final { fail "n_3.c: handling of comments"                      } }
 */

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/n_3.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-note: expected output: clang -E -P -std=c17, checked against mcpp's dg-final patterns
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: token 0 differs: expected 'abc', got 'abcde'
