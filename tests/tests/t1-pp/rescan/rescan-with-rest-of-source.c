// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-memory-full] 816-tcc fails with 'memory full' when an invocation's arguments continue past the end of a macro
#define f(x) x
#define g f
#define h g(
h 1) h (2))
// loomcc-expect: 1 (2)
