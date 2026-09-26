// loomcc-do: preprocess
#define f(x) x ## _suffix
#define g(x) prefix_ ## x
f(name) f() g(name) g()
// loomcc-expect: name_suffix _suffix prefix_name prefix_
