// loomcc-do: preprocess
#define foo foo
#define bar a bar b
foo bar
// loomcc-expect: foo a bar b
