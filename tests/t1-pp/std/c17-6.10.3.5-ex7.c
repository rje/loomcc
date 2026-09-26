// loomcc-do: preprocess
// loomcc-note: C17 6.10.3.5p9 EXAMPLE 7 (variadic macros)
#define debug(...) fprintf(stderr, __VA_ARGS__)
#define showlist(...) puts(#__VA_ARGS__)
#define report(test, ...) ((test)?puts(#test):\
 printf(__VA_ARGS__))
debug("Flag");
debug("X = %d\n", x);
showlist(The first, second, and third items.);
report(x>y, "x is %d but y is %d", x, y);
// loomcc-expect: fprintf(stderr, "Flag");
// loomcc-expect: fprintf(stderr, "X = %d\n", x);
// loomcc-expect: puts("The first, second, and third items.");
// loomcc-expect: ((x>y)?puts("x>y"): printf("x is %d but y is %d", x, y));
