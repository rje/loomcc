// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-hashhash-stringize] 816-tcc loses the ## made by pasting # and # when stringizing
// C17 6.10.3.3p4 EXAMPLE: a ## produced by pasting # and # is not an operator.
#define hash_hash # ## #
#define mkstr(a) # a
#define in_between(a) mkstr(a)
#define join(c, d) in_between(c hash_hash d)
char p[] = join(x, y);
// loomcc-expect: char p[] = "x ## y";
