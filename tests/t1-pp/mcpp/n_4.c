/* n_4.c:   Special tokens. */

/* 4.1: Digraph spellings in directive line.    */
/*  "abc";  */
%: define  stringize( a)    %: a

    stringize( abc);

/* 4.2: Digraph spellings are retained in stringization.    */
/*  "<:";   */
    stringize( <:);

/* { dg-do preprocess }
   { dg-options "-std=iso9899:199409" }
   { dg-final { if ![file exist n_4.i] { return }                       } }
   { dg-final { if \{ [grep n_4.i "\"abc\""] != ""              \} \{   } }
   { dg-final { if \{ [grep n_4.i "\"<:\""] != ""               \} \{   } }
   { dg-final { return \} \}                                            } }
   { dg-final { fail "n_4.c: digraphs conversion"                       } }
 */

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/n_4.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-note: expected output: clang -E -P -std=c17, checked against mcpp's dg-final patterns
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: token 0 differs: expected '"abc"', got '%:'
