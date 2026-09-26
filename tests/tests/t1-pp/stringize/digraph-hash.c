// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-no-digraphs] 816-tcc does not know digraphs
#define s(x) %:x
s(y)
// loomcc-expect: "y"
