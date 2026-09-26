static const unsigned char t[20000] = { 30, [19999] = 31 };
unsigned short far_sum2(unsigned short from, unsigned short n) { unsigned short s = 0; while (n--) s += t[from++]; return s; }
const unsigned char *far_ptr2(void) { const unsigned char *p = t; return p; }
