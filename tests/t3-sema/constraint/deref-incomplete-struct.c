// loomcc-do: syntax
struct Inc;
extern struct Inc *p;
int f(void) { return sizeof(*p); } // loomcc-error
