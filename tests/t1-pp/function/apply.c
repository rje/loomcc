// loomcc-do: preprocess
#define apply(m, a) m(a)
#define neg(x) -x
apply(neg, 3) apply(apply, (neg, 4))
// loomcc-expect: -3 apply((neg, 4))
