// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-char-spelling] 816-tcc respells ' as \' inside stringized literals
#define s(x) #x
s("'") s(a"b"c)
// loomcc-expect: "\"'\"" "a\"b\"c"
