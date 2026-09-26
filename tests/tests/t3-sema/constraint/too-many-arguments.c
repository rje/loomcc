// loomcc-do: syntax
int g(int a);
int f(void) { return g(1, 2); } // loomcc-error
