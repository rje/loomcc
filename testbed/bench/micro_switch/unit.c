/* switch: a dense switch over 0..11 (a jump-table candidate), a sparse one
 * over scattered values, and a small state machine driven by a byte stream. */

typedef unsigned short u16;
typedef unsigned char u8;

u16 dense(u16 op, u16 a, u16 b)
{
    switch (op) {
    case 0: return (u16)(a + b);
    case 1: return (u16)(a - b);
    case 2: return (u16)(a & b);
    case 3: return (u16)(a | b);
    case 4: return (u16)(a ^ b);
    case 5: return (u16)(a << 1);
    case 6: return (u16)(a >> 1);
    case 7: return (u16)~a;
    case 8: return (u16)(a + 1u);
    case 9: return (u16)(b - 1u);
    case 10: return a > b ? a : b;
    case 11: return a < b ? a : b;
    default: return 0xffffu;
    }
}

u16 sparse(u16 key)
{
    switch (key) {
    case 3: return 1;
    case 40: return 2;
    case 41: return 3;
    case 500: return 4;
    case 0x1000: return 5;
    case 0x7fff: return 6;
    case 0x8000: return 7;
    case 0xfffe: return 8;
    default: return 0;
    }
}

/* A tokenizer state machine: counts numbers, words and separators. */
#define S_IDLE 0
#define S_NUM 1
#define S_WORD 2
u16 tokens(const u8 *s, u16 n)
{
    u8 state = S_IDLE;
    u16 nums = 0, words = 0, seps = 0, i;
    for (i = 0; i < n; i++) {
        u8 c = s[i];
        u8 cls = (u8)((c >= '0' && c <= '9') ? S_NUM : (c >= 'a' && c <= 'z') ? S_WORD : S_IDLE);
        switch (state) {
        case S_IDLE:
            if (cls == S_NUM) nums++;
            else if (cls == S_WORD) words++;
            else seps++;
            state = cls;
            break;
        case S_NUM:
            if (cls == S_WORD) { words++; state = S_WORD; }
            else if (cls == S_IDLE) { seps++; state = S_IDLE; }
            break;
        case S_WORD:
            if (cls == S_IDLE) { seps++; state = S_IDLE; }
            break;
        }
    }
    return (u16)(nums | (words << 5) | (seps << 10));
}
