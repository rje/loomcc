// loomcc-do: preprocess
// loomcc-note: Translation phases: splices (2) before comments (3) before
// loomcc-note: directives and expansion (4).
#define A 1 /* comment with a splice \
still comment */ + 2
/\
* a comment started by a spliced slash-star *\
/
A
// loomcc-expect: 1 + 2
