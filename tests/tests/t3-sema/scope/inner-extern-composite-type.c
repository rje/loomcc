// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-inner-extern] 816-tcc keeps the outer, incomplete type
// 6.2.7p4 and 6.2.2p4: an inner-scope extern declaration that completes an
// array type gives the identifier the composite type in that scope, so the
// element type is complete there. Found through gcc.dg redecl-14.c.
typedef int IA[];
typedef int IA5[5];
extern IA *a[];
int f(void) {
  {
    extern IA5 *a[];
    return (int)sizeof(*a[0]);
  }
}
