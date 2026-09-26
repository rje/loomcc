// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-stray] 816-tcc rejects a backslash outside a literal
// A backslash outside a literal is not escaped (C17 6.10.3.2p2).
#define s(x) #x
s(: @\n)
// loomcc-expect: ": @\n"
