// loomcc-do: syntax
// loomcc-note: Old-style (K&R) definitions are obsolescent but still C17.
int add(a, b) int a; int b; { return a + b; }
int twice(x) int x; { return 2 * x; }
int use(void) { return add(1, 2) + twice(3); }
