// loomcc-do: preprocess
#if 1 // comment
a
#endif /* comment */
#if /* inside */ 1 /* more */
b
#endif
// loomcc-expect: a b
