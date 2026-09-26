/* Minimal <stdio.h> for wrapped external suites: what harness/rom/stdio.c
 * implements on the SNES (and the host C library provides elsewhere). */
#ifndef LOOMCC_LIBC_STDIO_H
#define LOOMCC_LIBC_STDIO_H
int printf(const char *fmt, ...);
int puts(const char *s);
int putchar(int c);
#define EOF (-1)
#endif
