// loomcc-do: preprocess
// loomcc-note: C17 6.10.8.1 mandatory macros exist.
#if defined(__DATE__) && defined(__FILE__) && defined(__LINE__) && defined(__STDC__) && defined(__STDC_VERSION__) && defined(__TIME__)
yes
#endif
// loomcc-expect: yes
