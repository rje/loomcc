// loomcc-do: preprocess
#if FOO == 0 && !BAR && (BAZ + 1) == 1
yes
#endif
// loomcc-expect: yes
