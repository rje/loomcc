// loomcc-do: syntax
struct P { int x, y; };
int *pi = (int[]){ 1, 2, 3 };
struct P *pp = &(struct P){ .x = 1 };
int f(void) { return ((struct P){ 3, 4 }).y + (int){ 5 }; }
