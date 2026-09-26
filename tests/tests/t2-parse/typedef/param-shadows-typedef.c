// loomcc-do: syntax
#include "loomcc-test.h"
typedef char T;
int f(int T) { return T * 2; }
int g(T x) { return x; }
STATIC_CHECK(sizeof(T) == 1);
