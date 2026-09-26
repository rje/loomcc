// loomcc-do: syntax
int g;
void f(void) { int i = &g; (void)i; } // loomcc-diagnostic
