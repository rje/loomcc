// loomcc-do: preprocess
// loomcc-timeout: 10
// loomcc-ref-diverges: tcc [tcc-line-macro-hang] 816-tcc hangs on #line with macro operands
#define N 300
#define F "macro.c"
#line N F
__LINE__ __FILE__
// loomcc-expect: 300 "macro.c"
