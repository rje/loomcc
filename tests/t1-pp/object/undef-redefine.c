// loomcc-do: preprocess
// loomcc-no-warnings
#define X 1
X
#undef X
X
#define X 2
X
#undef NEVER_DEFINED
// loomcc-expect: 1 X 2
