/* support file for far-code-and-data.c */
static const unsigned char t[20000] = { 10, [19999] = 11 };
unsigned short far_sum0(unsigned short from, unsigned short n) { unsigned short s = 0; while (n--) s += t[from++]; return s; }
const unsigned char *far_ptr0(void) { const unsigned char *p = t; return p; }
