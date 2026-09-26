// loomcc-do: preprocess
// <b.h> is a header-name only in #include; elsewhere it is < b . h >.
a <b.h> c
// loomcc-expect: a < b . h > c
