// loomcc-do: preprocess
// loomcc-note: C17 6.10.3.4p4 EXAMPLE: the result is unspecified, either
// loomcc-note: 2*9*g or 2*f(9). Prosser's algorithm, gcc and clang give 2*9*g,
// loomcc-note: and loomcc must match them.
#define f(a) a*g
#define g(a) f(a)
f(2)(9)
// loomcc-expect: 2*9*g
