// loomcc-do: preprocess
// A splice at the end of a // comment continues the comment (phase 2 before 3).
int a; // this comment continues \
int hidden;
int c;
// loomcc-expect: int a; int c;
