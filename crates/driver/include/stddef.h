#ifndef __LOOMCC_STDDEF_H
#define __LOOMCC_STDDEF_H
typedef unsigned int size_t;
typedef int ptrdiff_t;
typedef unsigned short wchar_t;
#define NULL ((void *)0)
#define offsetof(t, m) __builtin_offsetof(t, m)
#endif
