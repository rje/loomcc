// loomcc-do: syntax
// Case values are converted to the promoted controlling type: for an
// unsigned char, 256 converts to 0 and duplicates case 0? No: the
// controlling expression promotes to int, so 0 and 256 are distinct.
// loomcc-no-warnings
void f(unsigned char x) { switch (x) { case 0: break; case 256: break; } }
