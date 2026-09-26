// loomcc-do: syntax
// In C17, f() declares a function with unspecified parameters.
int f();
int g(void) { return f(1, 2) + f(); }
