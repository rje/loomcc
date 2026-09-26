// loomcc-do: preprocess
#define v(...) [__VA_ARGS__]
v() v( ) v(/**/)
// loomcc-expect: [] [] []
