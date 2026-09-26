// loomcc-do: preprocess
// loomcc-note: A # line inside macro arguments is undefined (6.10.3p11); gcc
// loomcc-note: and clang process it as a directive. Not tested: see PLAN.md.
#define f(x) x
f(1)
// loomcc-expect: 1
