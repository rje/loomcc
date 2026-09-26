// loomcc-do: syntax
typedef void (*cb)(int);
void reg(void (*f)(int), cb g, void (*table[4])(void), int (*(*h)(int))(char));
int (*select_fn(int k))(int, int);
int add(int a, int b) { return a + b; }
int (*select_fn(int k))(int, int) { (void)k; return add; }
