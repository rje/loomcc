/* Declarations PVSnesLib's libc.obj implements (816-tcc ABI). */
#ifndef LOOMCC_LIBC_STRING_H
#define LOOMCC_LIBC_STRING_H
#ifndef __SIZE_TYPE__
#define __SIZE_TYPE__ unsigned int
#endif
typedef __SIZE_TYPE__ size_t;
void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strcpy(char *dst, const char *src);
#endif
