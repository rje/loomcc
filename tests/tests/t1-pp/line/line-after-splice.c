// loomcc-do: preprocess
// loomcc-note: After a line splice the next line keeps its physical number.
a \
b
__LINE__
// loomcc-expect: a b 5
