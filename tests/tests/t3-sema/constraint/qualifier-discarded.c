// loomcc-do: syntax
// 6.5.16.1p1: the pointed-to type of the left must have all the qualifiers of the right.
const char *c;
char *p = c; // loomcc-diagnostic
