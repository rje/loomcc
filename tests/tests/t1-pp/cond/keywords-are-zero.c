// loomcc-do: preprocess
// Keywords are just identifiers in #if; they become 0.
#if int == 0 && sizeof == 0
yes
#endif
// loomcc-expect: yes
