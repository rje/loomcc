// loomcc-do: preprocess
// __DATE__ and __TIME__ expand to string literals (their text varies).
#define s(x) #x
#define xs(x) s(x)
#if defined(__DATE__) && defined(__TIME__)
yes
#endif
// loomcc-expect: yes
