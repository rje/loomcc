// loomcc-do: preprocess
// loomcc-note: C17 6.10.3.5p6 EXAMPLE 4 (stringizing and pasting); the
// loomcc-note: #include line is in include/std-6.10.3.5-ex4-include.c.
// loomcc-ref-diverges: tcc [tcc-stringize-space] 816-tcc drops the space in "strncmp(...) == 0"
#define str(s) # s
#define xstr(s) str(s)
#define debug(s, t) printf("x" # s "= %d, x" # t "= %s", \
 x ## s, x ## t)
#define INCFILE(n) vers ## n
#define glue(a, b) a ## b
#define xglue(a, b) glue(a, b)
#define HIGHLOW "hello"
#define LOW LOW ", world"
debug(1, 2);
fputs(str(strncmp("abc\0d", "abc", '\4') /* this goes away */
 == 0) str(: @\n), s);
glue(HIGH, LOW);
xglue(HIGH, LOW)
// loomcc-expect: printf("x" "1" "= %d, x" "2" "= %s", x1, x2);
// loomcc-expect: fputs("strncmp(\"abc\\0d\", \"abc\", '\\4') == 0" ": @\n", s);
// loomcc-expect: "hello";
// loomcc-expect: "hello" ", world"
