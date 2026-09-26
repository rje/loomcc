// loomcc-do: preprocess
// loomcc-note: C17 6.10.3.5p6 EXAMPLE 4, the #include line.
// loomcc-options: -Ih
#define str(s) # s
#define xstr(s) str(s)
#define INCFILE(n) vers ## n
#include xstr(INCFILE(2).h)
// loomcc-expect: int vers2;
