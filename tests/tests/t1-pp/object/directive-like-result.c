// loomcc-do: preprocess
// A macro that expands to `# define` does not make a directive.
#define H # define
H X 1
X
// loomcc-expect: # define X 1
// loomcc-expect: X
