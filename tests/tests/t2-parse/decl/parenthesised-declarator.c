// loomcc-do: syntax
#include "loomcc-test.h"
int (x) = 1;
int ((y)) = 2;
int (*(z)) = 0;
int (w)[2];
STATIC_CHECK(sizeof(w) == 2 * sizeof(int));
