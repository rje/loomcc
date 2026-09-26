// loomcc-do: preprocess
#define c3(a, b, c) a ## b ## c
c3(x, y, z) c3(1, 2, 3) c3(, y, ) c3(x, , z)
// loomcc-expect: xyz 123 y xz
