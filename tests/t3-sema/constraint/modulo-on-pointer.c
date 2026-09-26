// loomcc-do: syntax
int *p;
int f(void) { return (int)p % 2; }
int g(void) { return p % 2; } // loomcc-error
