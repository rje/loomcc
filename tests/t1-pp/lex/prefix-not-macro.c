// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-no-u8] 816-tcc has no u8 string prefix (C11)
// An encoding prefix glued to a literal is part of it, even if the name is a macro.
#define L 7
#define u8 x
#define U z
L"a" L "a" u8"s" u8 "s" U'c' U 'c'
// loomcc-expect: L"a" 7 "a" u8"s" x "s" U'c' z 'c'
