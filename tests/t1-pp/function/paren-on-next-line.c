// loomcc-do: preprocess
// Newlines between the name and ( are whitespace in an invocation.
#define f(x) [x]
f
(2)
f /* comment */ (3)
// loomcc-expect: [2] [3]
