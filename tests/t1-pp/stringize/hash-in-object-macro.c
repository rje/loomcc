// loomcc-do: preprocess
// In an object-like macro, # is an ordinary token.
#define H #
#define H2 # x
#define H3 a # b
H H2 H3
// loomcc-expect: # # x a # b
