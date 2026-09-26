#ifndef SELF_DEPTH
#define SELF_DEPTH 1
depth1
#include "self.h"
#elif SELF_DEPTH == 1
#undef SELF_DEPTH
#define SELF_DEPTH 2
depth2
#include "self.h"
#elif SELF_DEPTH == 2
#undef SELF_DEPTH
#define SELF_DEPTH 3
depth3
#endif
