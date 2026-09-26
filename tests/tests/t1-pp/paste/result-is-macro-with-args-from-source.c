// loomcc-do: preprocess
#define cat(a, b) a ## b
#define mac(x) <x>
cat(ma, c)(1) cat(ma, c)
(2)
// loomcc-expect: <1> <2>
