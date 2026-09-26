// loomcc-do: preprocess
#if defined __LINE__ && defined(__FILE__) && defined __STDC__
yes
#endif
#ifdef __LINE__
yes2
#endif
// loomcc-expect: yes yes2
