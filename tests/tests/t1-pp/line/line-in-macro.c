// loomcc-do: preprocess
// __LINE__ in a macro body is the line of the invocation.
#define L __LINE__
#define F(x) x __LINE__
L
F(1)
// loomcc-expect: 5 1 6
