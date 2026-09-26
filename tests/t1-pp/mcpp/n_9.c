/* n_9.t:   #pragma directive.  */

/* 9.1: Any #pragma directive should be processed or ignored, should not
    be diagnosed as an error.   */
#pragma __once
#pragma who knows ?

/* { dg-do preprocess } */

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/n_9.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-note: expected output: clang -E -P -std=c17, checked against mcpp's dg-final patterns
// loomcc-ref-diverges: tcc [tcc-mcpp] 816-tcc fails this mcpp test: token 0 differs: expected '#', got '<end>'
