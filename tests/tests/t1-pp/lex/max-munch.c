// loomcc-do: preprocess
a+++++b a---b a<<=b a>>=b a->b a...b a..b a.b
// loomcc-ref-diverges: tcc [tcc-dotdot] 816-tcc rejects `..` (two dots) as a token pair
// loomcc-expect: a ++ ++ + b a -- - b a <<= b a >>= b a -> b a ... b a . . b a . b
