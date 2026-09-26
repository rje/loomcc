/* n_32.c:  Escape sequence in character constant in #if expression.    */

/* 32.1:    Character octal escape sequence.    */
#if     '\123' != 83
#error  Bad evaluation of octal escape sequence.
#endif

/* 32.2:    Character hexadecimal escape sequence.  */
#if     '\x1b' != '\033'
#error  Bad evaluation of hexadecimal escape sequence.
#endif

/* { dg-do preprocess } */

// loomcc-do: preprocess
// loomcc-source: mcpp 2.7.2 cpp-test/test-t/n_32.c (BSD-2-Clause, see LICENSE in this directory)
// loomcc-note: expected output: clang -E -P -std=c17, checked against mcpp's dg-final patterns
