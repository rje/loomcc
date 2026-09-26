// loomcc-do: preprocess
#define A 2
#define B (A * 3)
#define F(x) ((x) + 1)
#if B == 6 && F(B) == 7
yes
#endif
// loomcc-expect: yes
