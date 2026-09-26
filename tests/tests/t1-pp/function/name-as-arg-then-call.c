// loomcc-do: preprocess
// f(f) yields a painted f: the following (1) does not invoke it.
#define f(x) x
f(f)(1)
// loomcc-expect: f(1)
