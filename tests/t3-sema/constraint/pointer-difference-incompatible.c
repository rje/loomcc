// loomcc-do: syntax
int a[2]; char b[2];
int f(void) { return (int)(a - b); } // loomcc-error
