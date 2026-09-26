// loomcc-do: syntax
int f(void);
int g(void) { return f(1); } // loomcc-error
