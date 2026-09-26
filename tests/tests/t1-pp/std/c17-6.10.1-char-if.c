// loomcc-do: preprocess
// loomcc-note: C17 6.10.1p4: character constants in #if (execution character
// loomcc-note: set is ASCII for every implementation here).
#if 'z' - 'a' == 25
yes
#endif
// loomcc-expect: yes
