// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-lax-decl] 816-tcc does not check enumerator ranges
// 6.7.2.2p2 (constraint): an enumeration constant's value must be
// representable as an int. loomcc panicked ("attempt to add with overflow")
// on LLONG_MAX (found through gcc.dg c11-enum-1.c).
enum big { BIG = 9223372036854775807LL }; // loomcc-diagnostic
