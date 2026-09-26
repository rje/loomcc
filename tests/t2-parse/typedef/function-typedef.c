// loomcc-do: syntax
typedef int Fn(int);
Fn impl;
int impl(int x) { return x + 1; }
Fn *ptr = impl;
