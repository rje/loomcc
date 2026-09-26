// loomcc-do: syntax
const int ci = 1;
volatile int vi;
const volatile int cvi = 2;
int *const cp = 0;
const int *pc;
int *volatile vp;
const int *const *volatile pcpv;
int *restrict rp;
void f(int *restrict a, const int *restrict b);
