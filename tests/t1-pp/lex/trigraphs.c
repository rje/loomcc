// loomcc-do: preprocess
// loomcc-note: C17 5.2.1.1 trigraphs. loomcc omits them on purpose (PLAN 2.1),
// loomcc-note: so this is an expected failure documenting the divergence.
// loomcc-xfail: trigraphs are deliberately unsupported (PLAN 2.1)
// loomcc-ref-diverges: tcc [tcc-no-trigraphs] 816-tcc does not replace trigraphs
??=define X ??( ??)
X "??!"
// loomcc-expect: [ ] "|"
