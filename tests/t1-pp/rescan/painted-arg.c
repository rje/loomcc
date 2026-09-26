// loomcc-do: preprocess
// foo is painted inside its own expansion and stays painted as an argument.
#define foo foo
#define f(x) x
f(foo) f(f(foo))
// loomcc-expect: foo foo
