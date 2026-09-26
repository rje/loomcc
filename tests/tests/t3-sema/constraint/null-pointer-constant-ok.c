// loomcc-do: syntax
// loomcc-no-warnings
// 0 and (void *)0 are null pointer constants: no diagnostic.
int *p = 0;
char *q = (void *)0;
void (*fp)(void) = 0;
int f(void) { return p == 0 && q != (void *)0 && !fp; }
