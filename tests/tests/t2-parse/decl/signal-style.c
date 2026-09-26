// loomcc-do: syntax
// The classic: void (*signal(int sig, void (*func)(int)))(int);
void (*signal(int sig, void (*func)(int)))(int);
void handler(int s) { (void)s; }
void use(void) { void (*old)(int) = signal(2, handler); (void)old; }
