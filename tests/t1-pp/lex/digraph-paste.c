// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-no-digraphs] 816-tcc does not know digraphs
%:define cat(a, b) a %:%: b
cat(x, y) cat(1, 2)
// loomcc-expect: xy 12
