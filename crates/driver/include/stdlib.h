#ifndef __LOOMCC_STDLIB_H
#define __LOOMCC_STDLIB_H
#include <stddef.h>
void exit(int status);
void abort(void);
int abs(int x);
void *malloc(size_t n);
void free(void *p);
void *calloc(size_t n, size_t m);
int atoi(const char *s);
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#endif
