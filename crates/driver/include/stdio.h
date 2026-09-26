#ifndef __LOOMCC_STDIO_H
#define __LOOMCC_STDIO_H
#include <stddef.h>
typedef struct __loomcc_file FILE;
extern FILE *stdout;
extern FILE *stderr;
int printf(const char *fmt, ...);
int fprintf(FILE *f, const char *fmt, ...);
int sprintf(char *s, const char *fmt, ...);
int putchar(int c);
int puts(const char *s);
#define EOF (-1)
#endif
