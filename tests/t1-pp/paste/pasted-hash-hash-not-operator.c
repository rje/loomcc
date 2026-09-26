// loomcc-do: preprocess
#define HH # ## #
#define f(x) x HH x
f(a)
// loomcc-expect: a ## a
