// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-char-spelling] 816-tcc -E respells character literals ('"' as '\"')
'"' "'" '\'' "\"" '\\' "\\" '\0' '\x7f' '\377'
// loomcc-expect: '"' "'" '\'' "\"" '\\' "\\" '\0' '\x7f' '\377'
