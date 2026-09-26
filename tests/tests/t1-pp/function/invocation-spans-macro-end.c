// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-memory-full] 816-tcc fails with 'memory full' when an invocation's arguments continue past the end of a macro
// The replacement ends with `f(`; the rest of the invocation comes from the source.
#define f(x) [x]
#define g f(
g 3)
// loomcc-expect: [3]
