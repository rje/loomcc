// loomcc-do: preprocess
// An identifier glued to a number is part of the number.
#define abc 1
12abc 1_abc abc12
// loomcc-expect: 12abc 1_abc abc12
