// loomcc-do: syntax
struct Inc;
extern struct Inc *p;
int f(void) { return p->x; } // loomcc-error
