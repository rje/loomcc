// loomcc-do: preprocess
#define OBJ a ## b
#define OBJ2 1 ## 2 ## 3
OBJ OBJ2
// loomcc-expect: ab 123
