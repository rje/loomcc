// loomcc-do: preprocess
// __LINE__ counts lines of the header while inside it, then resumes here.
#include "h/line.h"
__LINE__
// loomcc-expect: 1 2 4
