// loomcc-do: syntax
int *p;
int f(void) { return ~p != 0; } // loomcc-error
