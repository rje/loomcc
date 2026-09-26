// loomcc-do: syntax
int a[2];
int *f(void) { return a + a; } // loomcc-error
