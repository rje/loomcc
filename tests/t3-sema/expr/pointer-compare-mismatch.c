// loomcc-do: syntax
// 6.5.9p2 (constraint): comparing pointers to incompatible types.
int *p; char *c;
int f(void) { return p == c; } // loomcc-diagnostic
