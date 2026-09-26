// loomcc-do: syntax
// loomcc-no-warnings
int f(int);
int (*p1)(int) = f;
int (*p2)(int) = &f;
int (*p3)(int) = *f;
int (*p4)(int) = **f;
int g(void) { return p1(1) + (*p2)(2) + f(3) + (&f)(4); }
