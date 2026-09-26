// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-memory-full] 816-tcc fails with 'memory full' when an invocation's arguments continue past the end of a macro
// h's ( comes from a macro and its ) from the source; h is still invoked.
#define h(x) [x]
#define open h(
open 1) open (2))
// loomcc-expect: [1] [(2)]
