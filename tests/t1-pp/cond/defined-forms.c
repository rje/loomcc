// loomcc-do: preprocess
#define X
#if defined X && defined(X) && defined ( X ) && !defined Y && !defined(Y)
yes
#endif
// loomcc-expect: yes
