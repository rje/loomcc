/* loomcc-tests ROM harness: a minimal stdio for execute tests that print.
 *
 * Always compiled by 816-tcc and linked into every harness ROM, whichever
 * compiler built the test (calls from loomcc code use the 816-tcc ABI).
 * printf/puts/putchar append to loomcc_out_buf in WRAM; after the test's
 * main returns, harness.asm calls loomcc_check_output(), which compares the
 * buffer with the expected output the runner generated (expected.c).
 *
 * Integer widths follow loomcc: int 16 bits, `l` 32 bits (816-tcc's long is
 * 16 bits, so a %ld printed from 816-tcc code is that compiler's problem:
 * tcc-long16). `ll` is not supported.
 */
#include <stdarg.h>

/* A 32-bit integer: `long` for loomcc and msp430, `long long` for 816-tcc. */
#ifdef __TINYC__
typedef long long lt_long;
typedef unsigned long long lt_ulong;
#else
typedef long lt_long;
typedef unsigned long lt_ulong;
#endif


#define OUT_MAX 4096
char loomcc_out_buf[OUT_MAX];
unsigned short loomcc_out_len;
unsigned char loomcc_out_overflow;
unsigned char loomcc_diff[16];          /* actual bytes at the first mismatch */

extern const unsigned char loomcc_expected_out[];
extern const unsigned short loomcc_expected_len;
extern const unsigned char loomcc_expected_check;

int putchar(int c) {
  if (loomcc_out_len < OUT_MAX) loomcc_out_buf[loomcc_out_len++] = (char)c;
  else loomcc_out_overflow = 1;
  return c & 0xff;
}

int puts(const char *s) {
  while (*s) putchar(*s++);
  putchar('\n');
  return 1;
}

static int emit_num(lt_ulong v, int base, int upper, int neg, int width, int zero, int left) {
  char tmp[12];
  int n = 0, len, i;
  const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
  do { tmp[n++] = digits[v % base]; v /= base; } while (v);
  len = n + (neg ? 1 : 0);
  if (!left && !zero) for (i = len; i < width; i++) putchar(' ');
  if (neg) putchar('-');
  if (!left && zero) for (i = len; i < width; i++) putchar('0');
  while (n) putchar(tmp[--n]);
  if (left) for (i = len; i < width; i++) putchar(' ');
  return width > len ? width : len;
}

int vprintf(const char *fmt, va_list ap) {
  int count = 0;
  while (*fmt) {
    int zero = 0, left = 0, width = 0, lng = 0, prec = -1;
    char c = *fmt++;
    if (c != '%') { putchar(c); count++; continue; }
    /* (not `for (;; fmt++)`: 816-tcc miscompiles that, docs/TCC-BUGS.md) */
    while (*fmt == '0' || *fmt == '-' || *fmt == '+' || *fmt == ' ' || *fmt == '#') {
      if (*fmt == '0') zero = 1;
      if (*fmt == '-') left = 1;
      fmt++;
    }
    while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
    if (*fmt == '.') { fmt++; prec = 0; while (*fmt >= '0' && *fmt <= '9') prec = prec * 10 + (*fmt++ - '0'); }
    while (*fmt == 'l') { lng++; fmt++; }
    if (*fmt == 'h') { fmt++; if (*fmt == 'h') fmt++; }
    if (*fmt == 'z') fmt++;
    c = *fmt++;
    switch (c) {
    case 'd': case 'i': {
      lt_long v = lng ? va_arg(ap, lt_long) : (lt_long)va_arg(ap, int);
      count += emit_num(v < 0 ? (lt_ulong)-v : (lt_ulong)v, 10, 0, v < 0, width, zero, left);
      break;
    }
    case 'u': case 'x': case 'X': case 'o': {
      lt_ulong v = lng ? va_arg(ap, lt_ulong) : (lt_ulong)va_arg(ap, unsigned int);
      count += emit_num(v, c == 'u' ? 10 : c == 'o' ? 8 : 16, c == 'X', 0, width, zero, left);
      break;
    }
    case 'c': putchar(va_arg(ap, int)); count++; break;
    case 's': {
      const char *s = va_arg(ap, const char *);
      int n = 0, i;
      if (!s) s = "(null)";
      while (s[n] && (prec < 0 || n < prec)) n++;
      if (!left) for (i = n; i < width; i++) putchar(' ');
      for (i = 0; i < n; i++) putchar(s[i]);
      if (left) for (i = n; i < width; i++) putchar(' ');
      count += n > width ? n : width;
      break;
    }
    case 'p': {
      void *p = va_arg(ap, void *);
      putchar('0'); putchar('x');
      count += 2 + emit_num((lt_ulong)p, 16, 0, 0, 0, 0, 0);
      break;
    }
    case '%': putchar('%'); count++; break;
    default: putchar('%'); putchar(c); count += 2; break;
    }
  }
  return count;
}

int printf(const char *fmt, ...) {
  va_list ap;
  int n;
  va_start(ap, fmt);
  n = vprintf(fmt, ap);
  va_end(ap);
  return n;
}

/* 0 when the output matches (or nothing is expected); otherwise 1 + the
 * offset of the first difference, with loomcc_diff holding the actual
 * bytes from there. */
unsigned short loomcc_check_output(void) {
  unsigned short i, n;
  if (!loomcc_expected_check) return 0;
  n = loomcc_out_len < loomcc_expected_len ? loomcc_out_len : loomcc_expected_len;
  for (i = 0; i < n; i++)
    if ((unsigned char)loomcc_out_buf[i] != loomcc_expected_out[i]) break;
  if (i == n && loomcc_out_len == loomcc_expected_len && !loomcc_out_overflow) return 0;
  for (n = 0; n < 16; n++) loomcc_diff[n] = (unsigned char)(i + n < loomcc_out_len ? loomcc_out_buf[i + n] : 0);
  return i + 1;
}
