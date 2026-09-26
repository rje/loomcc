// loomcc-do: preprocess
// loomcc-note: Unbounded self-inclusion must end in a diagnostic, not a crash.
// loomcc-note: clang stops at depth 200.
// loomcc-timeout: 60
#include "h/forever.h" // loomcc-error@*: (nested|depth|deep|recursi)
