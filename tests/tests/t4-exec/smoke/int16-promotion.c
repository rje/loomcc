// loomcc-do: run
// loomcc-int: 16
void abort(void);
int main(void) {
  unsigned short a = 65535;
  unsigned int u = a + 1;      /* 16-bit int: unsigned short promotes to unsigned int */
  if (u != 0) abort();
  if (sizeof(int) != 2) abort();
  return 0;
}
