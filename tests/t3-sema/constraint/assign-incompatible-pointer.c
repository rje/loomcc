// loomcc-do: syntax
// 6.5.16.1p1 (constraint): pointers to incompatible types.
int *p;
char *q = p; // loomcc-diagnostic: (incompatible|pointer)
