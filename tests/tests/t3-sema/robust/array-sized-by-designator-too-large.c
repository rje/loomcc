// loomcc-do: syntax
// loomcc-ref:
// An array sized by its initializer is bounded by the 16 MiB address space
// too: a designator at 0x80000000 made loomcc lay out an 8 GiB object
// (gcc.dg large-size-array-2.c ran out of time).
static char *name[] = { [0x80000000] = "bar" }; // loomcc-error
static char ok[] = { [100] = 1 };
