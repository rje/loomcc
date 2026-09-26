// loomcc-do: preprocess
// loomcc-note: Hosted or freestanding is implementation-defined; the macro must exist.
// loomcc-ref-diverges: tcc [tcc-no-hosted] 816-tcc does not define __STDC_HOSTED__
#if defined(__STDC_HOSTED__) && (__STDC_HOSTED__ == 0 || __STDC_HOSTED__ == 1)
yes
#endif
// loomcc-expect: yes
