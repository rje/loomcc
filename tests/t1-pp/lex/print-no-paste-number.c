// loomcc-do: preprocess
// An identifier after a pp-number, or a number after an identifier, needs a space.
#define N 1
#define E e
N E N+E
// loomcc-expect: 1 e 1+e
