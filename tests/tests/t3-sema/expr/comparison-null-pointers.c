// loomcc-do: syntax
// loomcc-no-warnings
int *p; void *v; char *c;
int f(void) { return p == 0 && v == p && 0 == c && v != (void *)0 && !p; }
