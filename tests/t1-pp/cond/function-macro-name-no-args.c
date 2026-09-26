// loomcc-do: preprocess
// A function-like macro name not followed by ( is just an identifier (0).
#define F(x) x
#if F
wrong
#else
right
#endif
// loomcc-expect: right
