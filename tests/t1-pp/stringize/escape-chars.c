// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-char-spelling] 816-tcc respells '"' as '\"' before stringizing
#define s(x) #x
s('"') s('\'') s('\\') s('a')
// loomcc-expect: "'\"'" "'\\''" "'\\\\'" "'a'"
