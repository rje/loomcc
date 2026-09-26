// loomcc-do: preprocess
// loomcc-note: An unmatched " is undefined (6.4p3); every compiler here diagnoses it.
// loomcc-ref-diverges: tcc [tcc-lex-lax] 816-tcc -E passes the stray quote through
"abc // loomcc-diagnostic: (unterminated|missing|quote)
