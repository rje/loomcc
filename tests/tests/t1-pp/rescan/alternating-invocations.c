// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-E-glue] 816-tcc -E drops the space between separate tokens
// Each invocation's ( ) come from the source, so neither name stays painted.
#define foo() bar
#define bar() foo
foo()()() foo()()()()
// loomcc-expect: bar foo
