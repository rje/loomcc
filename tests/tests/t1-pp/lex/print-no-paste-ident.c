// loomcc-do: preprocess
#define a x
#define b y
#define f(p, q) p q
a b f(a, b) f(1, a)
// loomcc-expect: x y x y 1 x
