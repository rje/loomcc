// loomcc-do: syntax
// loomcc-no-warnings
int x;
const int cx = 1;
void *vp;
int *f(int c) { return c ? &x : 0; }
const int *g(int c) { return c ? &x : &cx; }
void *h(int c) { return c ? vp : &x; }
