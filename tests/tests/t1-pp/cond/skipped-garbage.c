// loomcc-do: preprocess
// loomcc-no-warnings
// Skipped groups are only scanned for directive names.
#if 0
#foo bar
#if garbage ( (
#else
#endif
#include <no/such/file.h>
#error not reached
#define X broken(
@ ` \
#elif garbage )
#endif
ok
// loomcc-expect: ok
