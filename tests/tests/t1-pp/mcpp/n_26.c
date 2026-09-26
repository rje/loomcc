/* n_26.c:  The name once replaced is not furthur replaced. */

/* 26.1:    Directly recursive object-like macro definition.    */
/*  Z(0);   */
#define Z   Z(0)
    Z;

/* 26.2:    Intermediately recursive object-like macro definition.  */
/*  AB; */
#define AB  BA
#define BA  AB
    AB;

/* 26.3:    Directly recursive function-like macro definition.  */
/*  x + f(x);   */
#define f(a)    a + f(a)
    f( x);

/* 26.4:    Intermediately recursive function-like macro definition.    */
/*  x + x + g( x);  */
#define g(a)    a + h( a)
#define h(a)    a + g( a)
    g( x);

/* 26.5:    Rescanning encounters the non-replaced macro name.  */
/*  Z(0) + f( Z(0));    */
    f( Z);

/* { dg-do preprocess }
   { dg-final { if ![file exist n_26.i] { return }                      } }
   { dg-final { if \{ [grep n_26.i "Z\\(0\\)"] != ""            \} \{   } }
   { dg-final { if \{ [grep n_26.i "AB"] != ""                  \} \{   } }
   { dg-final { if \{ [grep n_26.i "x *\\+ f\\( *x *\\)"] != "" \} \{   } }
   { dg-final { if \{ [grep n_26.i "x *\\+ *x *\\+ *g\\( *x *\\)"] != ""    \} \{   } }
   { dg-final { if \{ [grep n_26.i "Z\\(0\\) *\\+ f\\( *Z\\(0\\) *\\)"] != ""   \} \{   } }
   { dg-final { return \} \} \} \} \}                                   } }
   { dg-final { fail "n_26.c: recursive macro"                          } }
 */

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/n_26.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-note: expected output: clang -E -P -std=c17, checked against mcpp's dg-final patterns
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: token 27 differs: expected '+', got '('
