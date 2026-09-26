// loomcc-do: preprocess
// loomcc-note: The __VA_OPT__ examples of C23 6.10.5.1 (N3096 EXAMPLE 1-6).
// loomcc-ref-diverges: tcc [tcc-no-va-opt] 816-tcc has no __VA_OPT__
#define F(...) f(0 __VA_OPT__(,) __VA_ARGS__)
#define G(X, ...) f(0, X __VA_OPT__(,) __VA_ARGS__)
#define SDEF(sname, ...) S sname __VA_OPT__(= { __VA_ARGS__ })
#define EMP
F(a, b, c)
F()
F(EMP)
G(a, b, c)
G(a, )
G(a)
SDEF(foo);
SDEF(bar, 1, 2);
// loomcc-expect: f(0, a, b, c)
// loomcc-expect: f(0)
// loomcc-expect: f(0)
// loomcc-expect: f(0, a, b, c)
// loomcc-expect: f(0, a)
// loomcc-expect: f(0, a)
// loomcc-expect: S foo;
// loomcc-expect: S bar = { 1, 2 };
