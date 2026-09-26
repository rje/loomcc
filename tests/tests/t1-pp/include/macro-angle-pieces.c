// loomcc-do: preprocess
// loomcc-options: -Ii1
// loomcc-note: The header name is rebuilt from the tokens between < and >
// loomcc-note: (implementation-defined; gcc and clang join the spellings).
#define NAME angle
#define HDR <NAME.h>
#include HDR
// loomcc-expect: angle_from_i1
