// loomcc-do: syntax
// 6.2.2p7: an identifier with both internal and external linkage: undefined; diagnosed by clang.
// loomcc-ref-diverges: tcc [tcc-lax-decl] 816-tcc accepts it
int x;
static int x; // loomcc-diagnostic
