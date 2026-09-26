// loomcc-do: syntax
struct Incomplete;
int f(void) { return sizeof(struct Incomplete); } // loomcc-error
