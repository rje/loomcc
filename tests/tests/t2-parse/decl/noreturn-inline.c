// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-no-noreturn] 816-tcc has no _Noreturn
_Noreturn void die(void) { for (;;) {} }
static inline int twice(int x) { return 2 * x; }
inline int once(int x) { return x; }
extern int once(int x);
