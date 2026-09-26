// loomcc-do: preprocess
#define call(f, a) f a
#define sq(x) (x*x)
call(sq, (3)) call(sq,) (4)
// loomcc-expect: (3*3) (4*4)
