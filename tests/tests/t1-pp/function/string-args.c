// loomcc-do: preprocess
#define f(x) [x]
f("(,)") f(',') f(')') f("\")")
// loomcc-expect: ["(,)"] [','] [')'] ["\")"]
