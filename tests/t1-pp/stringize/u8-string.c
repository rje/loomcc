// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-no-u8] 816-tcc has no u8 prefix
#define s(x) #x
s(u8"a\\b") s(L'x')
// loomcc-expect: "u8\"a\\\\b\"" "L'x'"
