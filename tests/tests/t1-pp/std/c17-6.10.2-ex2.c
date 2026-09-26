// loomcc-do: preprocess
// loomcc-note: C17 6.10.2p8 EXAMPLE 2 (conditional include of a computed name).
// loomcc-options: -DVERSION=2 -I../include/h
#if VERSION == 1
#define INCFILE "vers1.h"
#elif VERSION == 2
#define INCFILE "vers2.h" // and so on
#else
#define INCFILE "versN.h"
#endif
#include INCFILE
// loomcc-expect: int vers2;
