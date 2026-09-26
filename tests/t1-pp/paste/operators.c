// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-hashhash-lex] 816-tcc cannot pass # as a macro argument to ##
#define cat(a, b) a ## b
cat(+, +) cat(<<, =) cat(-, >) cat(&, &) cat(|, =) cat(#, #)
// loomcc-expect: ++ <<= -> && |= ##
