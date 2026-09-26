// loomcc-do: preprocess
// A block comment spanning lines inside a directive keeps the directive going:
// comments become spaces in phase 3, directives run in phase 4.
#define X 1 /* starts here
   and ends here */ + 2
X
// loomcc-expect: 1 + 2
