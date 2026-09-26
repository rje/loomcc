// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-unknown-directive] 816-tcc ignores unknown directives
// loomcc-note: A non-directive in an active group is undefined; every
// loomcc-note: compiler here rejects it.
#foo bar // loomcc-diagnostic
