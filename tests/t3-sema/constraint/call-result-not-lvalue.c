// loomcc-do: syntax
int g(void);
void f(void) { g() = 3; } // loomcc-error
