// loomcc-do: preprocess
#if 10u == 10 && 10UL == 10 && 10LL == 10 && 10ull == 10 && 0x10 == 16 && 010 == 8 && 0 == 00
yes
#endif
// loomcc-expect: yes
