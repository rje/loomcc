// loomcc-do: syntax
#include "loomcc-test.h"
unsigned int a1; int unsigned a2; long unsigned int a3; int long unsigned a4;
const static int a5 = 1; static const int a6 = 1; int const static a7 = 1;
short int unsigned a8; char signed a9; long long int a10; int long long a11;
STATIC_CHECK(sizeof(a8) == sizeof(unsigned short));
STATIC_CHECK(sizeof(a3) == sizeof(unsigned long));
