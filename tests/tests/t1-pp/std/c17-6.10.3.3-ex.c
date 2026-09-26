// loomcc-do: preprocess
// loomcc-note: C17 6.10.3.3p4 EXAMPLE (see also paste/hash-hash-op.c).
// loomcc-ref-diverges: tcc [tcc-hashhash-stringize] 816-tcc loses the ## when stringizing
#define hash_hash # ## #
#define mkstr(a) # a
#define in_between(a) mkstr(a)
#define join(c, d) in_between(c hash_hash d)
char p[] = join(x, y);
// loomcc-expect: char p[] = "x ## y";
