// loomcc-do: preprocess
// Only the last token of the left and the first of the right are pasted.
#define f(x, y) x ## y
f(a b, c d) f(1 2, 3 4)
// loomcc-expect: a bc d 1 23 4
