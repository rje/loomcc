// loomcc-do: syntax
// 6.5.2.2p2 + 6.5.16.1: passing const char * to char * discards a qualifier.
void g(char *p);
void f(const char *s) { g(s); } // loomcc-diagnostic
