// loomcc-do: preprocess
// loomcc-note: #ident, #assert and #include_next are extensions; in a
// loomcc-note: skipped group they are ignored like any unknown directive.
#if 0
#ident "x"
#assert machine(snes)
#include_next <x.h>
#endif
ok
// loomcc-expect: ok
