// loomcc-do: preprocess
// loomcc-note: C23 6.10.5.1 EXAMPLE: __VA_OPT__ with ##.
// loomcc-ref-diverges: tcc [tcc-no-va-opt] 816-tcc has no __VA_OPT__
#define H2(X, Y, ...) __VA_OPT__(X ## Y,) __VA_ARGS__
H2(a, b, c, d)
#define H4(X, ...) __VA_OPT__(a X ## X) ## b
H4(, 1)
#define H5A(...) __VA_OPT__()/**/__VA_OPT__()
#define H5B(X) a ## X ## b
#define H5C(X) H5B(X)
H5C(H5A())
// loomcc-expect: ab, c, d
// loomcc-expect: a b
// loomcc-expect: ab
