// loomcc-do: preprocess
// loomcc-no-warnings
// A comment is whitespace, so this is fine.
#define X/**/1
X
// loomcc-expect: 1
