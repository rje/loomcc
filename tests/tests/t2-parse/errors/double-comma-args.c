// loomcc-do: syntax
int f(int a, int b);
int g(void) { return f(1,, 2); } // loomcc-error
