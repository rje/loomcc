// loomcc-do: run
// loomcc-int: 16
// Found by T6 stage 2 (Loom's game.c): a printf in a loop whose arguments are
// an array element and a call's result, after a call that writes a volatile.
// (loomcc's ROM printed the second argument's value for the first.)
int printf(const char *fmt, ...);
static unsigned short st = 44257u;
volatile unsigned short dbg = 44257u;
void seed(unsigned short s) { st = s != 0u ? s : 44257u; dbg = st; }
unsigned short state(void) { return st; }
int main(void) {
  static const unsigned short seeds[] = { 0, 1 };
  unsigned char i;
  for (i = 0; i < 2; i++) {
    seed(seeds[i]);
    printf("seed %u state %u\n", seeds[i], state());
  }
  return 0;
}
// loomcc-expect-stdout: seed 0 state 44257
// loomcc-expect-stdout: seed 1 state 1
