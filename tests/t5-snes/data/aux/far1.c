static const unsigned char t[20000] = { 20, [19999] = 21 };
unsigned short far_sum1(unsigned short from, unsigned short n) { unsigned short s = 0; while (n--) s += t[from++]; return s; }
const unsigned char *far_ptr1(void) { const unsigned char *p = t; return p; }
