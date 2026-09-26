// loomcc-do: preprocess
// f is not followed by ( when it is scanned, so it is not invoked.
#define LPAREN (
#define f(x) [x]
f LPAREN 1)
// loomcc-expect: f ( 1)
