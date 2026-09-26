// loomcc-do: preprocess
a +\
= b; c <<\
= d; e -\
> f
// loomcc-expect: a += b; c <<= d; e -> f
