// loomcc-do: preprocess
// loomcc-note: Implementation-defined (C17 6.10.1p4). Plain char is signed for
// loomcc-note: loomcc, 816-tcc and the hosts here, so '\377' is -1 in #if too.
#if '\377' < 0
signed
#else
unsigned
#endif
// loomcc-expect: signed
