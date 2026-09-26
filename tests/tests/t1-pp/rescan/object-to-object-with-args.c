// loomcc-do: preprocess
#define f(x) [x]
#define F f
#define G F
G(1) G G (2)
// loomcc-expect: [1] f [2]
