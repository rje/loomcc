// loomcc-do: preprocess
// loomcc-ref-diverges: tcc [tcc-E-glue] 816-tcc -E drops the space between separate tokens
// `.` next to a number from a macro must not become a pp-number.
#define D .
#define five 5
#define one 1
D five one D2 one.2
// loomcc-expect: . 5 1 D2 1 .2
