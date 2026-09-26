// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-hashhash-lex] 816-tcc -E loses the tokens after ###
// Outside a directive, # and ## are ordinary punctuators.
a ### b # ## #
// loomcc-expect: a ## # b # ## #
