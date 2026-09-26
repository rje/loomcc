/* n_21.c:  Tokenization (No preprocessing tokens are merged implicitly).   */

/* 21.1:    */
/*  - - -a; */
#define MINUS   -
    -MINUS-a;

/* 21.2:    */
#define sub( a, b)  a-b     /* '(a)-(b)' is better  */
#define Y   -y              /* '(-y)' is better     */
/*  x- -y;  */
    sub( x, Y);

/* { dg-do preprocess }
   { dg-final { if ![file exist n_21.i] { return }                      } }
   { dg-final { if \{ [grep n_21.i "- +- +- *a"] != ""          \} \{   } }
   { dg-final { if \{ [grep n_21.i "x *- +- *y"] != ""          \} \{   } }
   { dg-final { return \} \}                                            } }
   { dg-final { fail "n_21.c: tokenization of expanded macro"           } }
 */

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/n_21.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-note: expected output: clang -E -P -std=c17, checked against mcpp's dg-final patterns
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: token 0 differs: expected '-', got '--'
