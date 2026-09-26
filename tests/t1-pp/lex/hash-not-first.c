// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-hash-midline] 816-tcc treats a # in mid-line as a directive
// A # that is not the first token on a line does not start a directive.
a # define X 1
X
// loomcc-expect: a # define X 1
// loomcc-expect: X
