// loomcc-do: syntax
// else binds to the nearest if.
int f(int a, int b) { int r = 0; if (a) if (b) r = 1; else r = 2; return r; }
