// loomcc-do: syntax
// loomcc-no-warnings
static int f(void);
extern int f(void);
static int f(void) { return 1; }
int g(void) { return f(); }
